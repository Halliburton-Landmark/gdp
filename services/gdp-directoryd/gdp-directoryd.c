/*
**  GDP-DIRECTORYD handles (UDP) requests to add, find, or remove
**  "dguid,eguid" (destination guid, egress guid) tuples within the
**  gdp directory service's database (mariadb). This is an interim
**  "blackbox" to facilitate the transition to gdp net4...
**
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2017-2018, Regents of the University of California.
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
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mysql.h>
#include <errno.h>
#include <assert.h>
#include "gdp/gdp.h"
#include "gdp-directoryd.h"

// must match database configuration (see setup.mysql)
#define IDENTIFIED_BY "testblackbox"

#define GDP_NAME_HEX_FORMAT (2 * sizeof(gdp_name_t))
#define GDP_NAME_HEX_STRING (GDP_NAME_HEX_FORMAT + 1)
char dguid_s[GDP_NAME_HEX_STRING];
char eguid_s[GDP_NAME_HEX_STRING];

#define EXPIRE_TIMEOUT_SEC 60
char query_expire[] = "call blackbox.drop_expired();";

char call_add_nhop_pre[] = "call blackbox.add_nhop (x'";
char call_add_nhop_sep[] = "', x'";
char call_add_nhop_end[] = "');";

char call_delete_nhop_pre[] = "call blackbox.delete_nhop (x'";
char call_delete_nhop_sep[] = "', x'";
char call_delete_nhop_end[] = "');";

char call_find_nhop_pre[] = "call blackbox.find_nhop (x'";
char call_find_nhop_sep[] = "', x'";
char call_find_nhop_end[] = "');";

char call_flush_nhop_pre[] = "call blackbox.flush_nhops (x'";
char call_flush_nhop_end[] = "');";

// FIXME approximate size plus margin of error
#define GDP_QUERY_STRING (3 * (2 * sizeof(gdp_name_t)) + 256)
char query[GDP_QUERY_STRING];

otw_dir_t otw_dir;

void fail(MYSQL *con, char *s)
{
	perror(s);
	if (con)
		mysql_close(con);
	exit(1);
}

int main(int argc, char **argv)
{
	struct timeval tv = { .tv_sec = EXPIRE_TIMEOUT_SEC, .tv_usec = 0 };
	struct sockaddr_in si_loc;
	struct sockaddr_in si_rem;
	socklen_t si_rem_len = sizeof(si_rem);
	int fd_listen;
	int on = 1;
	int otw_dir_len;
	MYSQL *mysql_con;
	MYSQL_RES *mysql_result;
	unsigned int mysql_fields;
	MYSQL_ROW mysql_row;
	gdp_pname_t _tmp_pname_1;
	gdp_pname_t _tmp_pname_2;					

	if ((fd_listen = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		fail(NULL, "socket");

	// blocking sockets are sufficient for this simple/temp directory service
	// fcntl(fd_listen, F_SETFL, O_NONBLOCK);
	fcntl(fd_listen, F_SETFD, FD_CLOEXEC);
	assert(0 <=
		   setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)));
	// periodically remove inactive guids
	assert(0 <=
		   setsockopt(fd_listen, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));
	
	si_loc.sin_family = AF_INET;
	si_loc.sin_port = htons(PORT);
	si_loc.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd_listen, (struct sockaddr *)&si_loc, sizeof(si_loc)) < 0)
		fail(NULL, "bind");

	if ((mysql_con = mysql_init(NULL)) == NULL)
		fail(NULL, mysql_error(mysql_con));
  
	if (mysql_real_connect(mysql_con, "127.0.0.1", "gdpr", IDENTIFIED_BY,
						   NULL, 0, NULL, 0) == NULL)
		fail(mysql_con, mysql_error(mysql_con));

	while (1)
	{
		// await request (blocking)
		otw_dir_len = recvfrom(fd_listen, (uint8_t *) &otw_dir.ver,
							   sizeof(otw_dir), 0,
							   (struct sockaddr *)&si_rem, &si_rem_len);

		// timeout used to expire rows
		if (otw_dir_len < 0 && errno == EAGAIN)
		{
			debug(INFO, "\nprocess expired rows ");
			if (mysql_query(mysql_con, query_expire))
			{
				fprintf(stderr, "Error: query %s\n", mysql_error(mysql_con));
				fail(mysql_con, query_expire);
			}
			debug(INFO, "= %llu\n", mysql_affected_rows(mysql_con));

			// ensure "commands out of sync" error does not occur on next query
			mysql_result = mysql_store_result(mysql_con);
			if (mysql_result != NULL)
			{
				debug(INFO, "expired rows results found and freed\n");
				mysql_free_result(mysql_result);
				while (mysql_more_results(mysql_con))
				{
					debug(INFO, "expired rows more results found and freed\n");
					if (mysql_next_result(mysql_con) == 0)
					{
						mysql_result = mysql_store_result(mysql_con);
						mysql_free_result(mysql_result);
					}
				}
			}
			continue;
		}

		// continue if timeout or obviously short packets
		if (otw_dir_len < offsetof(otw_dir_t, dguid))
		{
			continue;
		}

		// currently, directory services are identical for v3 and v4 gdp
		if (otw_dir.ver != GDP_CHAN_PROTO_VERSION_4 &&
			otw_dir.ver != GDP_CHAN_PROTO_VERSION_3)
		{
			debug(WARN, "Warn: unrecognized packet from %s:%d len %d",
				  inet_ntoa(si_rem.sin_addr),
				  ntohs(si_rem.sin_port), otw_dir_len);
			continue;
		}
		debug(INFO, "\nReceived packet len %d from %s:%d\n", otw_dir_len,
			  inet_ntoa(si_rem.sin_addr), ntohs(si_rem.sin_port));

		// handle request
		switch (otw_dir.cmd)
		{

		case GDP_CMD_DIR_ADD:
		{
			char *q;

			debug(INFO, "id(0x%x) cmd -> add nhop\n"
				  "\teguid[%s]\n"
				  "\tdguid[%s]\n",
				  ntohs(otw_dir.id),
				  gdp_printable_name(otw_dir.eguid, _tmp_pname_1),
				  gdp_printable_name(otw_dir.dguid, _tmp_pname_2));

			// build the query

			strcpy(query, call_add_nhop_pre);
			q = query + sizeof(call_add_nhop_pre) - 1;

			debug(INFO, "-> eguid[");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(INFO, "%.2x", otw_dir.eguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.eguid[i]);
			}
			debug(INFO, "]\n");
			q += (2 * sizeof(gdp_name_t));

			strcat(q, call_add_nhop_sep);
			q += sizeof(call_add_nhop_sep) - 1;

			debug(INFO, "-> dguid[");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(INFO, "%.2x", otw_dir.dguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.dguid[i]);
			}
			debug(INFO, "]\n");
			q += (2 * sizeof(gdp_name_t));

			strcat(q, call_add_nhop_end);
			q += sizeof(call_add_nhop_end) - 1;
			
		}
		break;

		case GDP_CMD_DIR_DELETE:
		{
			char *q;

			debug(INFO, "id(0x%x) cmd -> delete nhop\n"
				  "\teguid[%s]\n"
				  "\tdguid[%s]\n",
				  ntohs(otw_dir.id),
				  gdp_printable_name(otw_dir.eguid, _tmp_pname_1),
				  gdp_printable_name(otw_dir.dguid, _tmp_pname_2));

			// build the query

			strcpy(query, call_delete_nhop_pre);
			q = query + sizeof(call_delete_nhop_pre) - 1;

			debug(INFO, "-> eguid[");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(INFO, "%.2x", otw_dir.eguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.eguid[i]);
			}
			debug(INFO, "]\n");
			q += (2 * sizeof(gdp_name_t));

			strcat(q, call_delete_nhop_sep);
			q += sizeof(call_delete_nhop_sep) - 1;

			debug(INFO, "-> dguid[");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(INFO, "%.2x", otw_dir.dguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.dguid[i]);
			}
			debug(INFO, "]\n");
			q += (2 * sizeof(gdp_name_t));

			strcat(q, call_delete_nhop_end);
			q += sizeof(call_delete_nhop_end) - 1;

		}
		break;

		case GDP_CMD_DIR_FIND:
		{
			char *q;
			
			debug(INFO, "id(0x%x) cmd -> find nhop\n"
				  "\teguid[%s]\n"
				  "\tdguid[%s]\n",
				  ntohs(otw_dir.id),
				  gdp_printable_name(otw_dir.eguid, _tmp_pname_1),
				  gdp_printable_name(otw_dir.dguid, _tmp_pname_2));

			// build the query

			strcpy(query, call_find_nhop_pre);
			q = query + sizeof(call_find_nhop_pre) - 1;

			debug(INFO, "-> eguid[");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(INFO, "%.2x", otw_dir.eguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.eguid[i]);
			}
			debug(INFO, "]\n");
			q += (2 * sizeof(gdp_name_t));
			
			strcat(q, call_find_nhop_sep);
			q += sizeof(call_find_nhop_sep) - 1;

			debug(INFO, "-> dguid[");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(INFO, "%.2x", otw_dir.dguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.dguid[i]);
			}
			debug(INFO, "]\n");
			q += (2 * sizeof(gdp_name_t));
			
			strcat(q, call_find_nhop_end);
			q += sizeof(call_find_nhop_end) - 1;
		}
		break;

		case GDP_CMD_DIR_FLUSH:
		{
			char *q;

			debug(INFO, "id(0x%x) cmd -> flush nhops\n"
				  "\teguid[%s]\n",
				  ntohs(otw_dir.id),
				  gdp_printable_name(otw_dir.eguid, _tmp_pname_1));

			// build the query

			strcpy(query, call_flush_nhop_pre);
			q = query + sizeof(call_flush_nhop_pre) - 1;

			debug(INFO, "-> eguid[");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(INFO, "%.2x", otw_dir.eguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.eguid[i]);
			}
			debug(INFO, "]\n");
			q += (2 * sizeof(gdp_name_t));

			strcat(q, call_flush_nhop_end);
			q += sizeof(call_flush_nhop_end) - 1;

		}
		break;

		default:
		{
			fprintf(stderr, "Error: packet with unknown cmd\n");
		}
		break;

		}

		if (mysql_query(mysql_con, query))
		{
			fprintf(stderr, "Error: query %s\n", mysql_error(mysql_con));
			fail(mysql_con, query);
		}
			
		mysql_result = mysql_store_result(mysql_con);

		if (otw_dir.cmd != GDP_CMD_DIR_FIND)
		{
			debug(INFO, "done (ack disabled)\n");
			/* otw_dir_len = sendto(fd_listen, (uint8_t *) &otw_dir.ver, */
			/* 					 otw_dir_len, 0, */
			/* 					 (struct sockaddr *)&si_rem, sizeof(si_rem)); */
		}
		else
		{
			if (mysql_result == NULL)
			{
				fprintf(stderr, "Error: result %s\n", mysql_error(mysql_con));
				fail(mysql_con, query);
			}
			else
			{
				mysql_fields = mysql_field_count(mysql_con);
				mysql_row = mysql_fetch_row(mysql_result);
				if (!mysql_row || mysql_fields != 1 || mysql_row[0] == NULL)
				{
					debug(INFO, "not found\n");
					memset(&otw_dir.eguid[0], 0, sizeof(gdp_name_t));
				}
				else
				{
					memcpy(&otw_dir.eguid[0], (uint8_t *) mysql_row[0],
						   sizeof(gdp_name_t));

					otw_dir.cmd = GDP_CMD_DIR_FOUND;
					
					debug(INFO, "\teguid[%s]\n",
						  gdp_printable_name(otw_dir.eguid, _tmp_pname_1));

					debug(INFO, "<- eguid[");
					for (int i = 0; i < sizeof(gdp_name_t); i++)
					{
						debug(INFO, "%.2x", (uint8_t) otw_dir.eguid[i]);
					}
					debug(INFO, "]\n");
				}
			}

			otw_dir_len = sendto(fd_listen, (uint8_t *) &otw_dir.ver,
								 otw_dir_len, 0,
								 (struct sockaddr *)&si_rem, sizeof(si_rem));
		}

		// ensure "commands out of sync" error does not occur on next query
		if (mysql_result != NULL)
		{
			mysql_free_result(mysql_result);
			while (mysql_more_results(mysql_con))
			{
				debug(INFO, "more results found and freed\n");
				if (mysql_next_result(mysql_con) == 0)
				{
					mysql_result = mysql_store_result(mysql_con);
					mysql_free_result(mysql_result);
				}
			}
		}

		if (otw_dir_len < 0)
		{
			debug(ERR, "Error: id(0x%x) send len %d error %s\n",
				  ntohs(otw_dir.id), otw_dir_len, strerror(errno));
		}
		else
		{
			debug(INFO, "id(0x%x) send len %d\n",
				  ntohs(otw_dir.id), otw_dir_len);
		}
	}

	// unreachable at the moment, but leave as a reminder to future expansion
	mysql_close(mysql_con);
	close(fd_listen);
	exit(0);
}
