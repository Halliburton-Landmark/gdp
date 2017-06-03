/* vim: set ai sw=4 sts=4 ts=4 :*/
/*
**	RESTful interface to GDP
**
**		Seriously prototype.
**
**		Uses SCGI between the web server and this process.	We link
**		with Sam Alexander's SCGI C Library, http://www.xamuel.com/scgilib/
**
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015-2017, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**	----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_hash.h>
#include <ep/ep_dbg.h>
#include <ep/ep_pcvt.h>
#include <ep/ep_sd.h>
#include <ep/ep_stat.h>
#include <ep/ep_xlate.h>
#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>	//XXX violates the principle that gdp-rest is "just an app"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sysexits.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <sys/socket.h>
#include <scgilib/scgilib.h>
#include <event2/event.h>
#include <jansson.h>
#include <sys/types.h>
#include <sys/wait.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.rest", "RESTful interface to GDP");

#define DEF_URI_PREFIX	"/gdp/v1"

const char		*GclUriPrefix;			// prefix on all REST calls
EP_HASH			*OpenGclCache;			// cache of open GCLs

gdp_gcl_open_info_t *shared_gcl_open_info;

// most gcl-create parameters are available through gdp-rest
#define GCL_C_PARAM_SERV_e		0
#define GCL_C_PARAM_SERV_K		1
#define GCL_C_PARAM_SERV_MAX	2
#define GCL_C_PARAM_MAX			(GCL_C_PARAM_SERV_MAX + 7)

const char *exec_gcl_create_param[GCL_C_PARAM_MAX] =
{
	/* 2 server controlled: */ "-e", "-K",
	/* 7 client options:  */ "-C", "-D", "-h", "-k", "-b", "-c", "-s"
	/* options not supported via RESTful interface: "-q" */
};
const char *exec_gcl_create = "/usr/bin/gcl-create";

// WARNING: gdp-rest parses gcl-create text output (which will be kept stable!)
#define GCL_C_OUT_GCL_NAME      16
#define GCL_C_OUT_GCL_NAME_END  59
#define GCL_C_OUT_LOGD_NAME     75
#define GCL_C_OUT_BUF_SIZE     256 // likely output * 2, rounded up
// gcl-create text output expectation (which will be kept stable!)
const char *gcl_c_min_out =
	"Created new GCL 0123456789012345678901234567890123456789012\n" //60
	"\ton log server N\n"; // N is a variable length > 0 log server name

/*
**  LOG_ERROR --- generic error logging routine
*/

void
log_error(const char *fmt, ...)
{
	va_list av;

	va_start(av, fmt);
	vfprintf(stderr, fmt, av);
	fprintf(stderr, ": %s\n", strerror(errno));
}

/*
**	SCGI_METHOD_NAME --- return printable name of method
**
**		Arguably this should be in SCGILIB.
*/

const char *
scgi_method_name(types_of_methods_for_http_protocol meth)
{
	switch (meth)
	{
		case SCGI_METHOD_UNSPECIFIED:
			return "unspecified";
		case SCGI_METHOD_UNKNOWN:
			return "unknown";
		case SCGI_METHOD_GET:
			return "GET";
		case SCGI_METHOD_POST:
			return "POST";
		case SCGI_METHOD_PUT:
			return "PUT";
		case SCGI_METHOD_DELETE:
			return "DELETE";
		case SCGI_METHOD_HEAD:
			return "HEAD";
	}
	return "impossible";
}


void
write_scgi(scgi_request *req,
		char *sbuf)
{
	int dead = 0;
	int i;
	char xbuf[1024];
	char *xbase = xbuf;
	char *xp;
	char *sp;

	// first translate any lone newlines into crlfs (pity jansson won't do this)
	for (i = 0, sp = sbuf; (sp = strchr(sp, '\n')) != NULL; sp++)
		if (sp == sbuf || sp[-1] != '\r')
			i++;

	// i is now the count of newlines without carriage returns
	sp = sbuf;
	if (i > 0)
	{
		bool cr = false;

		// find the total number of bytes we need, using malloc if necessary
		i += strlen(sbuf) + 1;
		if (i > sizeof xbuf)
			xbase = ep_mem_malloc(i);
		xp = xbase;

		// xp now points to a large enough buffer, possibly malloced
		while (*sp != '\0')
		{
			if (*sp == '\n' && !cr)
				*xp++ = '\r';
			cr = *sp == '\r';
			*xp++ = *sp++;
		}
		*xp = '\0';

		sp = xbase;
	}


	// I don't quite understand what "dead" is all about.  It's copied
	// from the "helloworld" example.  Something about memory management,
	// but his example only seems to use it to print messages.
	req->dead = &dead;
	i = scgi_write(req, sp);

	// free buffer memory if necessary
	if (xbase != xbuf)
		ep_mem_free(xbase);

	ep_dbg_cprintf(Dbg, 10, "scgi_write => %d, dead = %d\n", i, dead);
	if (i == 0)
	{
		char obuf[40];
		char nbuf[40];

		strerror_r(errno, nbuf, sizeof nbuf);
		ep_app_error("scgi_write (%s) failed: %s",
				ep_pcvt_str(obuf, sizeof obuf, sbuf), nbuf);
	}
	else if (dead)
	{
		ep_dbg_cprintf(Dbg, 1, "dead is set\n");
	}
	req->dead = NULL;
}


EP_STAT
gdp_failure(scgi_request *req, char *code, char *msg, char *fmt, ...)
{
	char buf[SCGI_MAX_OUTBUF_SIZE];
	va_list av;
	char c;
	json_t *j;
	char *jbuf;

	// set up the JSON object
	j = json_object();
	json_object_set_nocheck(j, "error", json_string(msg));
	json_object_set_nocheck(j, "code", json_string(code));
	json_object_set_nocheck(j, "uri", json_string(req->request_uri));
	json_object_set_nocheck(j, "method",
			json_string(scgi_method_name(req->request_method)));

	va_start(av, fmt);
	while ((c = *fmt++) != '\0')
	{
		char *key = va_arg(av, char *);

		switch (c)
		{
		case 's':
			json_object_set(j, key, json_string(va_arg(av, char *)));
			break;

		case 'd':
			json_object_set(j, key, json_integer((long) va_arg(av, int)));
			break;

		default:
			{
				char pbuf[40];

				snprintf(pbuf, sizeof pbuf, "Unknown format `%c'", c);
				json_object_set(j, key, json_string(pbuf));
			}
			break;
		}
	}
	va_end(av);

	// get it in string format
	jbuf = json_dumps(j, JSON_INDENT(4));

	// create the entire SCGI return message
	snprintf(buf, sizeof buf,
			"HTTP/1.1 %s %s\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"%s\r\n",
			code, msg, jbuf);
	write_scgi(req, buf);

	// clean up
	json_decref(j);
	free(jbuf);

	// should chose something more appropriate here
	return EP_STAT_ERROR;
}


/*
**  GDP_SCGI_RECV --- guarded version of scgi_recv().
**
**		The SCGI library isn't reentrant, so we have to avoid conflict
**		here.
*/

EP_THR_MUTEX	ScgiRecvMutex	EP_THR_MUTEX_INITIALIZER;

scgi_request *
gdp_scgi_recv(void)
{
	scgi_request *req;

	ep_thr_mutex_lock(&ScgiRecvMutex);
	req = scgi_recv();
	ep_thr_mutex_unlock(&ScgiRecvMutex);
	return req;
}


bool
is_integer_string(const char *s)
{
	do
	{
		if (!isdigit(*s))
			return false;
	} while (*++s != '\0');
	return true;
}


/*
**  PARSE_QUERY --- break up the query into key/value pairs
*/

struct qkvpair
{
	char	*key;
	char	*val;
};

int
parse_query(char *qtext, struct qkvpair *qkvs, int nqkvs)
{
	int n = 0;

	for (n = 0; qtext != NULL && n < nqkvs; n++)
	{
		char *p;

		if (*qtext == '\0')
			break;

		// skip to the next kv pair separator
		qkvs[n].key = strsep(&qtext, "&;");

		// separate the key and the value (if any)
		p = strchr(qkvs[n].key, '=');
		if (p != NULL && *p != '\0')
			*p++ = '\0';
		qkvs[n].val = p;
	}

	return n;
}


/*
**  FIND_QUERY_KV --- find a key-value pair in query
*/

char *
find_query_kv(const char *key, struct qkvpair *qkvs)
{
	while (qkvs->key != NULL)
	{
		if (strcasecmp(key, qkvs->key) == 0)
			return qkvs->val;
		qkvs++;
	}
	return NULL;
}


/*
**  ERROR400 --- issue a 400 "Bad Request" error
*/

EP_STAT
error400(scgi_request *req, const char *detail)
{
	return gdp_failure(req, "400", "Bad Request", "s", "detail", detail);
}

/*
**  ERROR404 --- issue a 404 "Not Found" error
*/

EP_STAT
error404(scgi_request *req, const char *detail)
{
	return gdp_failure(req, "404", "Not Found", "s",
			"detail", detail);
}


/*
**  ERROR405 --- issue a 405 "Method Not Allowed" error
*/

EP_STAT
error405(scgi_request *req, const char *detail)
{
	return gdp_failure(req, "405", "Method Not Allowed", "s",
			"detail", detail);
}

/*
**  ERROR409 --- issue a 409 "Conflict" error
*/

EP_STAT
error409(scgi_request *req, const char *detail)
{
	return gdp_failure(req, "409", "Conflict", "s",	"detail", detail);
}

/*
**  ERROR500 --- issue a 500 "Internal Server Error" error
*/

EP_STAT
error500(scgi_request *req, const char *detail, int eno)
{
	char nbuf[40];

	strerror_r(eno, nbuf, sizeof nbuf);
	(void) gdp_failure(req, "500", "Internal Server Error", "ss",
			"errno", nbuf,
			"detail", detail);
	return ep_stat_from_errno(eno);
}


/*
**  ERROR501 --- issue a 501 "Not Implemented" error
*/

EP_STAT
error501(scgi_request *req, const char *detail)
{
	return gdp_failure(req, "501", "Not Implemented", "s",
			"detail", detail);
}


/*
**  PROCESS_GCL_CREATE_REQ --- create new GCL via gcl-create, parse stdout msg
*/

EP_STAT
process_gcl_create_req(scgi_request *req, const char *gclxname,
					   json_t *j, json_t *j_meta, size_t options_max)
{
	EP_STAT estat = EP_STAT_OK;
	int opt;
	const char *options[options_max];
	size_t j_unprocessed;
	size_t j_meta_unprocessed;
	int o_pipe[2];
	pid_t pid;
	int status;
	int exit_code;
	json_t *j_temp;
	json_t *j_resp;
	char *jbuf;
	char sbuf[SCGI_MAX_OUTBUF_SIZE];
        int p;
	
	// mandatory gclxname json obj already processed by caller to arrive here
	j_unprocessed = json_object_size(j) - 1;

	// process name in slot zero
	opt = 0;
	options[opt++] = exec_gcl_create;
		
	// then add server controlled parameters
	options[opt++] = exec_gcl_create_param[GCL_C_PARAM_SERV_e];
	options[opt++] = "none";
	options[opt++] = exec_gcl_create_param[GCL_C_PARAM_SERV_K];
	options[opt++] = "/etc/gdp/keys";

	// then add client requested options
	for (p = GCL_C_PARAM_SERV_MAX; p < GCL_C_PARAM_MAX; p++)
	{
		// borrowed
		if ((j_temp = json_object_get(j, exec_gcl_create_param[p])) != NULL)
		{
			options[opt++] = exec_gcl_create_param[p];
			// borrowed
			options[opt++] = json_string_value(j_temp);
			if (options[opt - 1] != NULL)
				j_unprocessed--;
		}
	}

	// then add client optional "META" array elements
	j_meta_unprocessed = json_array_size(j_meta);
	if (j_meta_unprocessed > 0)
	{
		
		size_t j_meta_i;

		json_array_foreach(j_meta, j_meta_i, j_temp)
		{
			if (json_is_string(j_temp))
			{
				// borrowed
				options[opt] = json_string_value(j_temp);
				if (options[opt] != NULL && strchr(options[opt], '=') != NULL)
				{
					opt++;
					j_meta_unprocessed--;
				}
			}
		}
		// json key "META" has been processed
		j_unprocessed--;
	}

	// then add the external-name if present (implies HTTP PUT)
	if (gclxname != NULL)
		options[opt++] = gclxname;
		
	// and finally terminate the argv-style options pointer array
	options[opt++] = NULL;

	// unprocessed client content is treated as an error
	if (j_unprocessed > 0 || j_meta_unprocessed > 0)
	{
		estat = error400(req, "request contains unrecognized json objects");
		return estat;
	}

	// prep to capture child's stdout
	if ((pipe(o_pipe)) == -1)
	{
		estat = error500(req, "gdp-rest pipe", errno);
		return estat;
	}

	if ((pid = fork()) == -1)
	{
		estat = error500(req, "gdp-rest fork", errno);
		close(o_pipe[STDOUT_FILENO]);
		close(o_pipe[STDIN_FILENO]);
		return estat;
	}

	if (pid == 0)
	{
		// child moves child side of pipe to own stdout
		dup2(o_pipe[STDOUT_FILENO], STDOUT_FILENO);
		close(o_pipe[STDOUT_FILENO]);
		// child closes parent side of pipe
		close(o_pipe[STDIN_FILENO]);
		// child execv gcl-create
		execv(exec_gcl_create, (char * const*)options);
		// child execv launch failed
		perror("gdp-rest execv failure");
		exit(EXIT_FAILURE);
	}

	// parent closes child side of pipe
	close(o_pipe[STDOUT_FILENO]);

	// wait for child
	if (waitpid(pid, &status, 0) != pid)
	{
		estat = error500(req, "gdp-rest waitpid", errno);
		close(o_pipe[STDIN_FILENO]);
		return estat;
	}

	// check child status for unexpected exits
	if (! WIFEXITED(status) ||
		(exit_code = WEXITSTATUS(status)) == EXIT_FAILURE)
	{
		estat = gdp_failure(req, "500", "Internal Server Error", "sds",
							"detail", "gdp-rest execv gcl-create failure",
							"status", status,
							"request", req->body);
		close(o_pipe[STDIN_FILENO]);
		return estat;
	}
	ep_dbg_cprintf(Dbg, 5, "gdp-rest exec gcl-create exited(%d)\n", exit_code);
	
	if (exit_code == EX_OK)
	{
		ssize_t bytes;
		char obuf[GCL_C_OUT_BUF_SIZE];

		bytes = read(o_pipe[STDIN_FILENO], obuf, sizeof obuf - 1);
		if (bytes < 0)
		{
			estat = error500(req, "gdp-rest pipe read", errno);
			close(o_pipe[STDIN_FILENO]);
			return estat;
		}
		obuf[bytes] = '\0';
		ep_dbg_cprintf(Dbg, 5, "pipe read %ld bytes = {\n%s}\n", bytes, obuf);
		if (bytes < sizeof gcl_c_min_out)
		{
			estat = gdp_failure(req, "500", "Internal Server Error", "ss",
								"detail", "gdp-rest pipe read bytes",
								"read", obuf);
			close(o_pipe[STDIN_FILENO]);
			return estat;
		}

		// new
		if ((j_resp = json_object()) == NULL)
		{
			estat = error500(req, "gdp-rest response", ENOMEM);
			close(o_pipe[STDIN_FILENO]);
			return estat;
		}

		// gcl-create EX_OK output is kept stable to permit string extraction
		obuf[GCL_C_OUT_GCL_NAME_END] = '\0'; // terminate a newline of line 1
		obuf[bytes - 1] = '\0';	// terminate at newline of line 2

		// new
		j_temp = json_string(&obuf[GCL_C_OUT_GCL_NAME]);
		if (j_temp == NULL)
		{
			estat = error500(req, "gdp-rest response gcl_name", EIO);
			json_decref(j_resp);
			close(o_pipe[STDIN_FILENO]);
			return estat;
		}
		// steal j_temp
		if ((json_object_set_new_nocheck(j_resp, "gcl_name", j_temp)) == -1)
		{
			estat = gdp_failure(req, "500", "Internal Server Error", "ss",
								"detail", "gdp-rest response",
								"gcl_name", &obuf[GCL_C_OUT_GCL_NAME]);
			json_decref(j_temp);
			json_decref(j_resp);
			close(o_pipe[STDIN_FILENO]);
			return estat;
		}
		// new
		j_temp = json_string(&obuf[GCL_C_OUT_LOGD_NAME]);
		if (j_temp == NULL)
		{
			estat = error500(req, "gdp-rest response logd_name", EIO);
			json_decref(j_resp);
			close(o_pipe[STDIN_FILENO]);
			return estat;
		}
		// steal j_temp
		if ((json_object_set_new_nocheck(j_resp, "gdplogd_name", j_temp)) == -1)
		{
			estat = gdp_failure(req, "500", "Internal Server Error", "ss",
								"detail", "gdp-rest response",
								"gdplogd_name", &obuf[GCL_C_OUT_LOGD_NAME]);
			json_decref(j_temp);
			json_decref(j_resp);
			close(o_pipe[STDIN_FILENO]);
			return estat;
		}
		// malloc
		if ((jbuf = json_dumps(j_resp, JSON_INDENT(4))) == NULL)
		{
			estat = error500(req, "gdp-rest response reformat", EIO);
			json_decref(j_resp);
			close(o_pipe[STDIN_FILENO]);
			return estat;
		}
		snprintf(sbuf, sizeof sbuf,
				 "HTTP/1.1 201 GCL created\r\n"
				 "Content-Type: application/json\r\n"
				 "\r\n"
				 "%s\r\n",
				 jbuf);
		write_scgi(req, sbuf);
		free(jbuf);
		json_decref(j_resp);
	}
	else if ((exit_code == EX_CANTCREAT) &&
			 (req->request_method == SCGI_METHOD_PUT))
		estat = error409(req, "external-name already exists on gdplogd server");
	else if (exit_code == EX_CANTCREAT)
		estat = error409(req, "generated-name conflict on gdplogd server");
	else if (exit_code == EX_NOHOST)
		estat = error400(req, "log server host not found");
	else if (exit_code == EX_UNAVAILABLE)
		estat = error400(req, "key length selection insecure, denied");
	else
		estat = gdp_failure(req, "500", "Internal Server Error", "sd",
							"detail", "gcl-create unexpected error",
							"exit_code", exit_code);

	close(o_pipe[STDIN_FILENO]);
	return estat;
}


/*
**  A_NEW_GCL --- create new GCL
*/

EP_STAT
a_new_gcl(scgi_request *req)
{
	EP_STAT estat = EP_STAT_OK;
	json_t *j;
	const char *gclxname;
	size_t options_max;
	json_t *j_temp;
	json_t *j_meta;
	
	ep_dbg_cprintf(Dbg, 5, "=== Create new GCL (%s)\n", req->body);

	// new
	if ((j = json_loads(req->body, JSON_REJECT_DUPLICATES, NULL)) == NULL)
	{
		estat = error400(req, "request body not recognized json format");
		return estat;
	}
	
	// mandatory external-name obj, to prevent URL crawl-driven log creation
	// borrowed
	if ((j_temp = json_object_get(j, "external-name")) == NULL)
	{
		estat = error400(req, "mandatory external-name not found");
		json_decref(j);
		return estat;
	}
	// borrowed
	gclxname = json_string_value(j_temp);

	// external-name obj value must be NULL for POST, non-NULL for PUT
	if ((req->request_method == SCGI_METHOD_POST && gclxname != NULL) ||
		(req->request_method == SCGI_METHOD_PUT && gclxname == NULL))
	{
		if (gclxname != NULL)
			estat = error400(req, "POST external-name must have null value");
		else
			estat = error400(req, "PUT external-name must have non-null value");
		json_decref(j);
		return estat;
	}

	// procname + 2 * (server params + client params) + glxname + terminator
	options_max = 1 + 2 * (GCL_C_PARAM_SERV_MAX + json_object_size(j)) + 1 + 1;

	// peek at META array now, to size options array, then set aside for later
	// borrowed
	if (((j_meta = json_object_get(j, "META")) != NULL) &&
		json_is_array(j_meta))
	{
		options_max += json_array_size(j_meta);
	}

	estat = process_gcl_create_req(req, gclxname, j, j_meta, options_max);

	json_decref(j);
	return estat;
}

/*
**  A_SHOW_GCL --- show information about a GCL
*/

EP_STAT
a_show_gcl(scgi_request *req, gdp_name_t gcliname)
{
	return error501(req, "GCL status not implemented");
}


/*
**  A_APPEND --- append datum to GCL
*/

EP_STAT
a_append(scgi_request *req, gdp_name_t gcliname, gdp_datum_t *datum)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_gcl_t *gcl = NULL;

	ep_dbg_cprintf(Dbg, 5, "=== Append value to GCL\n");

	estat = gdp_gcl_open(gcliname, GDP_MODE_AO, shared_gcl_open_info, &gcl);
	EP_STAT_CHECK(estat, goto fail_open);

	estat = gdp_gcl_append(gcl, datum);
	EP_STAT_CHECK(estat, goto fail_append);
	
	{
		// success: send a response
		char rbuf[SCGI_MAX_OUTBUF_SIZE];
		json_t *j = json_object();
		char *jbuf;
		EP_TIME_SPEC ts;

		json_object_set_nocheck(j, "recno",
								json_integer(gdp_datum_getrecno(datum)));
		gdp_datum_getts(datum, &ts);
		if (EP_TIME_IS_VALID(&ts))
		{
			char tbuf[100];

			ep_time_format(&ts, tbuf, sizeof tbuf, EP_TIME_FMT_DEFAULT);
			json_object_set_nocheck(j, "timestamp", json_string(tbuf));
		}
		jbuf = json_dumps(j, JSON_INDENT(4));

		snprintf(rbuf, sizeof rbuf,
				"HTTP/1.1 200 Successfully appended\r\n"
				"Content-Type: application/json\r\n"
				"\r\n"
				"%s\r\n",
				jbuf);
		write_scgi(req, rbuf);

		// clean up
		free(jbuf);
		json_decref(j);
	}

	// finished
	gdp_gcl_close(gcl);
	// caller frees datum
	return estat;
	
 fail_append:
	gdp_gcl_close(gcl);
 fail_open:
	// caller frees datum
	{
		char ebuf[200];
		gdp_pname_t gclpname;

		gdp_printable_name(gcliname, gclpname);
		gdp_failure(req, "420", "Cannot append to GCL", "ss",
				"GCL", gclpname,
				"error", ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	A_READ_DATUM --- read and return a datum from a GCL
**
**		XXX Currently doesn't use the GCL cache.  To make that work
**			long term we would have to have to implement LRU in that
**			cache (which we probably need to do anyway).
*/

EP_STAT
a_read_datum(scgi_request *req, gdp_name_t gcliname, gdp_recno_t recno)
{
	EP_STAT estat;
	gdp_gcl_t *gcl = NULL;
	gdp_datum_t *datum = gdp_datum_new();

	estat = gdp_gcl_open(gcliname, GDP_MODE_RO, shared_gcl_open_info, &gcl);
	EP_STAT_CHECK(estat, goto fail_open);

	estat = gdp_gcl_read(gcl, recno, datum);
	if (!EP_STAT_ISOK(estat))
		goto fail_read;

	// package up the results and send them back
	{
		char rbuf[1024];

		// figure out the response header
		{
			FILE *fp;
			gdp_pname_t gclpname;
			EP_TIME_SPEC ts;

			fp = ep_fopen_smem(rbuf, sizeof rbuf, "w");
			if (fp == NULL)
			{
				char nbuf[40];

				strerror_r(errno, nbuf, sizeof nbuf);
				ep_app_abort("Cannot open memory for GCL read response: %s",
						nbuf);
			}
			gdp_printable_name(gcliname, gclpname);
			fprintf(fp, "HTTP/1.1 200 GCL Message\r\n"
						"Content-Type: application/json\r\n"
						"GDP-GCL-Name: %s\r\n"
						"GDP-Record-Number: %" PRIgdp_recno "\r\n",
						gclpname,
						datum->recno);
			gdp_datum_getts(datum, &ts);
			if (EP_TIME_IS_VALID(&ts))
			{
				fprintf(fp, "GDP-Commit-Timestamp: ");
				ep_time_print(&ts, fp, EP_TIME_FMT_DEFAULT);
				fprintf(fp, "\r\n");
			}
			fprintf(fp, "\r\n");				// end of header
			fputc('\0', fp);
			fclose(fp);
		}

		// finish up sending the data out --- the extra copy is annoying
		{
			size_t rlen = strlen(rbuf);
			size_t dlen = evbuffer_get_length(gdp_datum_getbuf(datum));
			char obuf[1024];
			char *obp = obuf;

			if (rlen + dlen > sizeof obuf)
				obp = ep_mem_malloc(rlen + dlen);

			if (obp == NULL)
			{
				char nbuf[40];

				strerror_r(errno, nbuf, sizeof nbuf);
				ep_app_abort("Cannot allocate memory for GCL read response: %s",
						nbuf);
			}

			memcpy(obp, rbuf, rlen);
			gdp_buf_read(gdp_datum_getbuf(datum), obp + rlen, dlen);
			scgi_send(req, obp, rlen + dlen);
			if (obp != obuf)
				ep_mem_free(obp);
		}
	}

	// finished
	gdp_datum_free(datum);
	gdp_gcl_close(gcl);
	return estat;

 fail_read:
	gdp_gcl_close(gcl);
 fail_open:
	gdp_datum_free(datum);
	{
		char ebuf[200];
		gdp_pname_t gclpname;

		gdp_printable_name(gcliname, gclpname);
		gdp_failure(req, "404", "Cannot read GCL", "ss",
				"GCL", gclpname,
				"reason", ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  GCL_DO_GET --- helper routine for GET method on a GCL
**
**		Have to look at query to figure out the semantics.
**		The query's the thing / wherein I'll catch the
**		conscience of the king.
*/

EP_STAT
gcl_do_get(scgi_request *req, gdp_name_t gcliname, struct qkvpair *qkvs)
{
	EP_STAT estat;
	char *qrecno = find_query_kv("recno", qkvs);
	char *qnrecs = find_query_kv("nrecs", qkvs);
	char *qtimeout = find_query_kv("timeout", qkvs);

	if (qnrecs != NULL)
	{
		// not yet implemented
		estat = error501(req, "nrecs query not supported");
	}
	else if (qtimeout != NULL)
	{
		// not yet implemented
		estat = error501(req, "timeout query not supported");
	}
	else if (qrecno != NULL)
	{
		gdp_recno_t recno;

		if (strcmp(qrecno, "last") == 0)
			recno = -1;
		else
			recno = atol(qrecno);
		estat = a_read_datum(req, gcliname, recno);
	}
	else
	{
		estat = a_show_gcl(req, gcliname);
	}

	return estat;
}


/*
**  PFX_GCL --- process SCGI requests starting with /gdp/v1/gcl
*/

#define NQUERY_KVS		10		// max number of key-value pairs in query part

EP_STAT
pfx_gcl(scgi_request *req, char *uri)
{
	EP_STAT estat;
	struct qkvpair qkvs[NQUERY_KVS + 1];

	if (*uri == '/')
		uri++;

	ep_dbg_cprintf(Dbg, 3, "    gcl=%s\n    query=%s\n", uri, req->query_string);

	// parse the query, if it exists
	{
		int i = parse_query(req->query_string, qkvs, NQUERY_KVS);
		qkvs[i].key = qkvs[i].val = NULL;
	}

	if (*uri == '\0')
	{
		// URI does not include a GCL name, implies GDP scoped operations
		switch (req->request_method)
		{
		case SCGI_METHOD_POST:
			// fall-through
		case SCGI_METHOD_PUT:
			// create a new GCL
			estat = a_new_gcl(req);
			break;

		case SCGI_METHOD_GET:
			// XXX if no GCL name, should we print all GCLs?
			estat = error404(req, "listing GCLs not implemented (yet)");
			break;

		default:
			// unknown URI/method
			estat = error405(req,
							 "only GET, POST, or PUT supported in GDP scope");
			break;
		}
	}
	else
	{
		gdp_name_t gcliname;

		// next component is the GCL id (name) in external format
		gdp_parse_name(uri, gcliname);

		// URI includes a GCL name, implies GCL scoped operations
		switch (req->request_method)
		{
		case SCGI_METHOD_GET:
			estat = gcl_do_get(req, gcliname, qkvs);
			break;

		case SCGI_METHOD_POST:
			// append value to GCL
			{
				gdp_datum_t *datum = gdp_datum_new();

				gdp_buf_write(gdp_datum_getbuf(datum), req->body,
							  req->scgi_content_length);
				estat = a_append(req, gcliname, datum);
				gdp_datum_free(datum);
			}
			break;

		default:
			// unknown URI/method
			estat = error405(req, "only GET or POST supported in GCL scope");
		}
	}

	return estat;
}


/*
**  PFX_POST --- process SCGI requests starting with /gdp/v1/post
*/

EP_STAT
pfx_post(scgi_request *req, char *uri)
{
	if (*uri == '/')
		uri++;

	return error501(req, "/post/... URIs not yet supported");
}



/**********************************************************************
**
**  KEY-VALUE STORE
**
**		This implementation is very sloppy right now.
**
**********************************************************************/


json_t			*KeyValStore = NULL;
gdp_gcl_t		*KeyValGcl = NULL;
const char		*KeyValStoreName;
gdp_name_t		KeyValInternalName;


EP_STAT
insert_datum(gdp_datum_t *datum)
{
	if (datum == NULL)
		return GDP_STAT_INTERNAL_ERROR;

	gdp_buf_t *buf = gdp_datum_getbuf(datum);
	size_t len = gdp_buf_getlength(buf);
	unsigned char *p = gdp_buf_getptr(buf, len);

	json_t *j = json_loadb((char *) p, len, 0, NULL);
	if (!json_is_object(j))
		return GDP_STAT_MSGFMT;

	json_object_update(KeyValStore, j);
	json_decref(j);
	return EP_STAT_OK;
}


EP_STAT
kv_initialize(void)
{
	EP_STAT estat;
	gdp_event_t *gev;

	if (KeyValStore != NULL)
		return EP_STAT_OK;

	// get space for our internal database
	KeyValStore = json_object();

	KeyValStoreName = ep_adm_getstrparam("swarm.rest.kvstore.gclname",
							"swarm.rest.kvstore.gclname");

	// open the "KeyVal" GCL
	gdp_parse_name(KeyValStoreName, KeyValInternalName);
	estat = gdp_gcl_open(KeyValInternalName, GDP_MODE_AO, NULL, &KeyValGcl);
	EP_STAT_CHECK(estat, goto fail0);

	// read all the data
	estat = gdp_gcl_multiread(KeyValGcl, 1, 0, NULL, NULL);
	EP_STAT_CHECK(estat, goto fail1);
	while ((gev = gdp_event_next(KeyValGcl, 0)) != NULL)
	{
		if (gdp_event_gettype(gev) == GDP_EVENT_EOS)
		{
			// end of multiread --- we have it all
			gdp_event_free(gev);
			break;
		}
		else if (gdp_event_gettype(gev) == GDP_EVENT_DATA)
		{
			// update the current stat of the key-value store
			estat = insert_datum(gdp_event_getdatum(gev));
		}
		gdp_event_free(gev);
	}

	// now start up the subscription (will be read in main loop)
	estat = gdp_gcl_subscribe(KeyValGcl, 0, 0, NULL, NULL, NULL);
	goto done;

fail0:
	// couldn't open; try create?
	//estat = gdp_gcl_create(KeyValInternalName, NULL, &KeyValGcl);

fail1:
	// couldn't read GCL

done:
	return estat;
}


EP_STAT
pfx_kv(scgi_request *req, char *uri)
{
	EP_STAT estat;

	if (*uri == '/')
		uri++;

	if (KeyValStore == NULL)
	{
		estat = kv_initialize();
		EP_STAT_CHECK(estat, goto fail0);
	}

	if (req->request_method == SCGI_METHOD_POST)
	{
		// get the datum out of the SCGI request
		gdp_datum_t *datum = gdp_datum_new();
		gdp_buf_write(gdp_datum_getbuf(datum), req->body, req->scgi_content_length);

		// try to merge it into the in-memory representation
		estat = insert_datum(datum);

		// if that succeeded, append the record to the GCL
		if (EP_STAT_ISOK(estat))
			a_append(req, KeyValInternalName, datum);

		// don't forget to mop up!
		gdp_datum_free(datum);
	}
	else if (req->request_method == SCGI_METHOD_GET)
	{
		FILE *fp;
		char rbuf[2000];

		fp = ep_fopen_smem(rbuf, sizeof rbuf, "w");
		if (fp == NULL)
		{
			char nbuf[40];

			strerror_r(errno, nbuf, sizeof nbuf);
			ep_app_abort("Cannot open memory for GCL read response: %s",
					nbuf);
		}

		json_t *j0 = json_object_get(KeyValStore, uri);
		if (j0 == NULL)
			return error404(req, "No such key");

		json_t *j1 = json_object();
		json_object_set(j1, uri, j0);
		fprintf(fp, "HTTP/1.1 200 Data\r\n"
					"Content-Type: application/json\r\n"
					"\r\n"
					"%s\r\n",
					json_dumps(j1, 0));
		fputc('\0', fp);
		fclose(fp);
		json_decref(j1);

		scgi_send(req, rbuf, strlen(rbuf));
	}
	else
	{
		return error405(req, "unknown method");
	}

	return EP_STAT_OK;

fail0:
	return error500(req, "Couldn't initialize Key-Value store", errno);
}


void
process_event(gdp_event_t *gev)
{
	if (gdp_event_gettype(gev) != GDP_EVENT_DATA)
		return;

	if (KeyValStore == NULL)
		return;

	// for the time being, assume it comes from the correct GCL
	insert_datum(gdp_event_getdatum(gev));
}


/**********************************************************************/

/*
**	PROCESS_SCGI_REQ --- process an already-read SCGI request
**
**		This is generally called as an event
*/

EP_STAT
process_scgi_req(scgi_request *req)
{
	char *uri;				// the URI of the request
	EP_STAT estat = EP_STAT_OK;

	if (ep_dbg_test(Dbg, 3))
	{
		ep_dbg_printf("Got connection on port %d from %s:\n",
				req->descriptor->port->port, req->remote_addr);
		ep_dbg_printf("	   %s %s\n", scgi_method_name(req->request_method),
				req->request_uri);
	}

	// strip query string off of URI (I'm surprised it's not already done)
	uri = strchr(req->request_uri, '?');
	if (uri != NULL)
		*uri = '\0';

	// strip off leading "/gdp/v1/" prefix (error if not there)
	if (GclUriPrefix == NULL)
		GclUriPrefix = ep_adm_getstrparam("swarm.rest.prefix", DEF_URI_PREFIX);
	uri = req->request_uri;
	if (strncmp(uri, GclUriPrefix, strlen(GclUriPrefix)) != 0)
	{
		estat = error404(req, "improper URI prefix");
		goto finis;
	}
	uri += strlen(GclUriPrefix);
	if (*uri == '/')
		uri++;

	// next component is "gcl" for RESTful or "post" for RESTish
	if (strncmp(uri, "gcl", 3) == 0 && (uri[3] == '/' || uri[3] == '\0'))
	{
		// looking at "/gdp/v1/gcl/" prefix; next component is the GCL name
		estat = pfx_gcl(req, uri + 3);
	}
	else if (strncmp(uri, "post", 4) == 0 && (uri[4] == '/' || uri[4] == '\0'))
	{
		// looking at "/gdp/v1/post" prefix
		estat = pfx_post(req, uri + 4);
	}
	else if (strncmp(uri, "kv", 2) == 0 && (uri[2] == '/' || uri[4] == '\0'))
	{
		// looking at "/gdp/v1/kv" prefix
		estat = pfx_kv(req, uri + 2);
	}
	else
	{
		// looking at "/gdp/v1/<unknown>" prefix
		estat = error404(req, "unknown resource");
	}

finis:
	return estat;
}


int
main(int argc, char **argv, char **env)
{
	int opt;
	int listenport = -1;
	int64_t poll_delay;
	char *gdpd_addr = NULL;
	bool show_usage = false;
	extern void run_scgi_protocol(void);

	while ((opt = getopt(argc, argv, "D:G:p:u:C:")) > 0)
	{
		switch (opt)
		{
		case 'D':						// turn on debugging
			ep_dbg_set(optarg);
			break;

		case 'G':						// gdp daemon host:port
			gdpd_addr = optarg;
			break;

		case 'p':						// select listen port
			listenport = atoi(optarg);
			break;

		case 'u':						// URI prefix
			GclUriPrefix = optarg;
			break;

		case 'C':						// (development) gcl-create alternative
			exec_gcl_create = optarg;
			break;

		default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc > 0)
	{
		fprintf(stderr,
				"Usage: %s [-D dbgspec] [-G host:port] [-p port] [-u prefix] "
				"[-C gcl-create]\n", ep_app_getprogname());
		exit(EX_USAGE);
	}

	if (listenport < 0)
		listenport = ep_adm_getintparam("swarm.rest.scgi.port", 8001);

	// Initialize the GDP library
	//		Also initializes the EVENT library and starts the I/O thread
	// Initialize shared gcl open info with caching enabled
	{
		EP_STAT estat = gdp_init(gdpd_addr);
		char ebuf[100];

		if (!EP_STAT_ISOK(estat))
		{
			ep_app_abort("Cannot initialize gdp library: %s",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
		}

		shared_gcl_open_info = gdp_gcl_open_info_new();
		estat = gdp_gcl_open_info_set_caching(shared_gcl_open_info, true);

		if (!EP_STAT_ISOK(estat))
		{
			ep_app_abort("Cannot initialize gdp gcl cache preference: %s",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
		}
	}
		
	ep_dbg_cprintf(Dbg, 9, "GDP initialized\n");

	// Initialize SCGI library
	scgi_debug = ep_dbg_level(Dbg) / 10;
	if (scgi_initialize(listenport))
	{
		ep_dbg_cprintf(Dbg, 1,
				"%s: listening for SCGI on port %d, scgi_debug %d\n",
				ep_app_getprogname(), listenport, scgi_debug);
	}
	else
	{
		char nbuf[40];

		strerror_r(errno, nbuf, sizeof nbuf);
		ep_app_error("could not initialize SCGI port %d: %s",
				listenport, nbuf);
		return EX_OSERR;
	}

	// start looking for SCGI connections
	//	XXX This should really be done through the event library
	//		rather than by polling.	 To do this right there should
	//		be a pool of worker threads that would have the SCGI
	//		connection handed off to them.
	//	XXX May be able to cheat by changing the select() in
	//		scgi_update_connections_port to wait.  It's OK if this
	//		thread hangs since the other work happens in a different
	//		thread.
	poll_delay = ep_adm_getlongparam("swarm.rest.scgi.pollinterval", 100000);
	ep_sd_notifyf("READY=1\n");
	for (;;)
	{
		gdp_event_t *gev;
		EP_TIME_SPEC to = { 0, 0, 0.0 };

		while ((gev = gdp_event_next(NULL, &to)) != NULL)
		{
			process_event(gev);
			gdp_event_free(gev);
		}

		scgi_request *req = gdp_scgi_recv();
		int dead = 0;

		if (req == NULL)
		{
			(void) ep_time_nanosleep(poll_delay * 1000LL);
			continue;
		}
		req->dead = &dead;
		process_scgi_req(req);
	}
}
