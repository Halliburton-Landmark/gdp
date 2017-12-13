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
**	Copyright (c) 2017, Regents of the University of California.
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
#include <gdp/gdp.h>
#include "gdp-directoryd.h"

#define GDP_NAME_HEX_FORMAT (2 * sizeof(gdp_name_t))
#define GDP_NAME_HEX_STRING (GDP_NAME_HEX_FORMAT + 1)
char dguid_s[GDP_NAME_HEX_STRING];
char eguid_s[GDP_NAME_HEX_STRING];

char query_replace_prefix[] = "replace into blackbox.gdpd values (x'";
char query_replace_mid[] = "', x'";
char query_replace_sep[] = "'), (x'";
char query_replace_end[] = "');";

char query_delete_prefix[] =
	"delete from blackbox.gdpd where (dguid,eguid) in ((x'";
char query_delete_mid[] = "', x'";
char query_delete_sep[] = "'), (x'";
char query_delete_end[] = "'));";

char query_find_prefix[] =
	"select eguid from blackbox.gdpd where dguid = x'";
char query_find_end[] = "' ;";

// FIXME rough num
#define GDP_QUERY_STRING ((2 + DIR_OGUID_MAX) * 2 * sizeof(gdp_name_t) + 2048)
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
	int oguid_len;
	
	// sanity check compiler directive is operational
	assert(sizeof(otw_dir_t) == OTW_DIR_SIZE_ASSERT);
	
	if ((fd_listen = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		fail(NULL, "socket");

	// blocking sockets are sufficient for this simple/temp directory service
	// fcntl(fd_listen, F_SETFL, O_NONBLOCK);
	fcntl(fd_listen, F_SETFD, FD_CLOEXEC);
	setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

	si_loc.sin_family = AF_INET;
	si_loc.sin_port = htons(PORT);
	si_loc.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd_listen, (struct sockaddr *)&si_loc, sizeof(si_loc)) < 0)
		fail(NULL, "bind");

	if ((mysql_con = mysql_init(NULL)) == NULL)
		fail(NULL, mysql_error(mysql_con));
  
	if (mysql_real_connect(mysql_con, "127.0.0.1", "gdpr", "testblackbox",
						   NULL, 0, NULL, 0) == NULL)
		fail(mysql_con, mysql_error(mysql_con));

	while (1)
	{
		// await request (blocking)
		otw_dir_len = recvfrom(fd_listen, (uint8_t *) &otw_dir.ver,
							   sizeof(otw_dir), 0,
							   (struct sockaddr *)&si_rem, &si_rem_len);

		// parse request
		if (otw_dir.ver != GDP_CHAN_PROTO_VERSION)
		{
			debug(WARN, "Warn: unrecognized packet from %s:%d len %d",
				  inet_ntoa(si_rem.sin_addr),
				  ntohs(si_rem.sin_port), otw_dir_len);
			continue;
		}
		debug(INFO, "Received packet len %d from %s:%d\n", otw_dir_len,
			  inet_ntoa(si_rem.sin_addr), ntohs(si_rem.sin_port));

		// handle request
		switch (otw_dir.cmd)
		{
		case GDP_CMD_DIR_FIND:
		{
			char *q;
			
			// one entry per key, so no multiple replies for now
			debug(INFO, "cmd -> find\n");

			// build the query

			strcpy(query, query_find_prefix);
			q = query + sizeof(query_find_prefix) - 1;

			debug(VERB, "-> dguid [");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(VERB, "%.2x", otw_dir.dguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.dguid[i]);
			}
			debug(VERB, "]\n");
			q += (2 * sizeof(gdp_name_t));
			
			strcat(q, query_find_end);
		}
		break;

		case GDP_CMD_DIR_ADD:
		{
			char *q;
			
			debug(INFO, "cmd -> add\n");

			oguid_len = otw_dir_len - offsetof(otw_dir_t, oguid);
			if (oguid_len % sizeof(gdp_name_t) != 0)
			{
				debug(WARN, "Warn: invalid oguid len %d", oguid_len);
				continue;
			}

			// build the query

			strcpy(query, query_replace_prefix);
			q = query + sizeof(query_replace_prefix) - 1;

			debug(VERB, "-> dguid [");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(VERB, "%.2x", otw_dir.dguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.dguid[i]);
			}
			debug(VERB, "]\n");
			q += (2 * sizeof(gdp_name_t));
			
			strcat(q, query_replace_mid);
			q += sizeof(query_replace_mid) - 1;

			debug(VERB, "-> eguid [");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(VERB, "%.2x", otw_dir.eguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.eguid[i]);
			}
			debug(VERB, "]\n");
			q += (2 * sizeof(gdp_name_t));

			debug(VERB, "oguid len %d\n", oguid_len);

			for (int d = 0; d < oguid_len; d += sizeof(gdp_name_t))
			{
				strcat(q, query_replace_sep);
				q += sizeof(query_replace_sep) - 1;
				
				debug(VERB, "-> dguid [");
				for (int i = 0; i < sizeof(gdp_name_t); i++)
				{
					debug(VERB, "%.2x", otw_dir.oguid[d + i]);
					sprintf(q + (i * 2), "%.2x", otw_dir.oguid[d + i]);
				}
				debug(VERB, "]\n");
				q += (2 * sizeof(gdp_name_t));

				strcat(q, query_replace_mid);
				q += sizeof(query_replace_mid) - 1;

				debug(VERB, "-> eguid [");
				for (int i = 0; i < sizeof(gdp_name_t); i++)
				{
					debug(VERB, "%.2x", otw_dir.eguid[i]);
					sprintf(q + (i * 2), "%.2x", otw_dir.eguid[i]);
				}
				debug(VERB, "]\n");
				q += (2 * sizeof(gdp_name_t));
			}
			
			strcat(q, query_replace_end);
			q += sizeof(query_replace_end) - 1;
		}
		break;

		case GDP_CMD_DIR_REMOVE:
		{
			char *q;
			
			debug(INFO, "cmd -> remove\n");

			oguid_len = otw_dir_len - offsetof(otw_dir_t, oguid);
			if (oguid_len % sizeof(gdp_name_t) != 0)
			{
				debug(WARN, "Warn: invalid oguid len %d", oguid_len);
				continue;
			}

			// build the query

			strcpy(query, query_delete_prefix);
			q = query + sizeof(query_delete_prefix) - 1;

			debug(VERB, "-> dguid [");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(VERB, "%.2x", otw_dir.dguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.dguid[i]);
			}
			debug(VERB, "]\n");
			q += (2 * sizeof(gdp_name_t));
			
			strcat(q, query_delete_mid);
			q += sizeof(query_delete_mid) - 1;

			debug(VERB, "-> eguid [");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				debug(VERB, "%.2x", otw_dir.eguid[i]);
				sprintf(q + (i * 2), "%.2x", otw_dir.eguid[i]);
			}
			debug(VERB, "]\n");
			q += (2 * sizeof(gdp_name_t));

			debug(VERB, "oguid len %d\n", oguid_len);

			for (int d = 0; d < oguid_len; d += sizeof(gdp_name_t))
			{
				strcat(q, query_delete_sep);
				q += sizeof(query_delete_sep) - 1;
				
				debug(VERB, "-> dguid [");
				for (int i = 0; i < sizeof(gdp_name_t); i++)
				{
					debug(VERB, "%.2x", otw_dir.oguid[d + i]);
					sprintf(q + (i * 2), "%.2x", otw_dir.oguid[d + i]);
				}
				debug(VERB, "]\n");
				q += (2 * sizeof(gdp_name_t));

				strcat(q, query_delete_mid);
				q += sizeof(query_delete_mid) - 1;

				debug(VERB, "-> eguid [");
				for (int i = 0; i < sizeof(gdp_name_t); i++)
				{
					debug(VERB, "%.2x", otw_dir.eguid[i]);
					sprintf(q + (i * 2), "%.2x", otw_dir.eguid[i]);
				}
				debug(VERB, "]\n");
				q += (2 * sizeof(gdp_name_t));
			}
			
			strcat(q, query_delete_end);
			q += sizeof(query_delete_end) - 1;
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
			
		// handle result
		mysql_result = mysql_store_result(mysql_con);

		if (otw_dir.cmd != GDP_CMD_DIR_FIND)
		{
			otw_dir_len = sendto(fd_listen, (uint8_t *) &otw_dir.ver,
								 otw_dir_len, 0,
								 (struct sockaddr *)&si_rem, sizeof(si_rem));
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
					debug(INFO, "dguid not found\n");
					// should already be zero from sender...
					// memset(&otw_dir.eguid[0], 0, sizeof(gdp_name_t));
				}
				else
				{
					debug(INFO, "<- eguid [");
					for (int c = 0; c < sizeof(gdp_name_t); c++)
					{
						debug(INFO, "%.2x", (uint8_t) mysql_row[0][c]);
						otw_dir.eguid[c] = (uint8_t) mysql_row[0][c];
					}
					debug(INFO, "]\n");
					// tell submitter to add eguid (response to find)
					otw_dir.cmd = GDP_CMD_DIR_FOUND;
				}
				// good or bad, free result now
				mysql_free_result(mysql_result);
			}

			otw_dir_len = sendto(fd_listen, (uint8_t *) &otw_dir.ver,
								 otw_dir_len, 0,
								 (struct sockaddr *)&si_rem, sizeof(si_rem));
		}

		if (otw_dir_len < 0)
			fprintf(stderr, "Error: sendto len %d error %s\n",
					otw_dir_len, strerror(errno));
		else
			debug(INFO, "Send len %d\n", otw_dir_len);
	}

	// unreachable at the moment, but leave as a reminder to future expansion
	mysql_close(mysql_con);
	close(fd_listen);
	exit(0);
}
