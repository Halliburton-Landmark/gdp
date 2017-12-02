/*
**  GDP-DIRECTORYD handles (UDP) requests to add, lookup, or remove
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

// FIXME temporary assignment
#define PORT 9001

// FIXME version 3 otw_pdu support for the moment...
#define GDP_CHAN_PROTO_VERSION 3

// FIXME temporary until cmd extensions designed and added to gdp_pdu.h
#define GDP_CMD_DIR_ADD		7
#define GDP_CMD_DIR_REMOVE	8
#define GDP_CMD_DIR_LOOKUP	9

// FIXME eventually maintain this in gdp_chan.h or other appropriate shared .h
#pragma pack(push, 1)
typedef struct otw_pdu_v3_s
{
	uint8_t ver;
	uint8_t ttl;
	uint8_t rsvd1;
	uint8_t cmd;
	uint8_t dst[sizeof(gdp_name_t)];
	uint8_t src[sizeof(gdp_name_t)];
} otw_pdu_v3_t;
#pragma pack(pop)

// on the wire pdu
#define OTW_PDU_SIZE 68 // sanity check otw_pdu structure size
otw_pdu_v3_t otw_pdu;

#define GDP_NAME_HEX_FORMAT (2 * sizeof(gdp_name_t))
#define GDP_NAME_HEX_STRING (GDP_NAME_HEX_FORMAT + 1)
#define GDP_QUERY_STRING    1024 // arbitrarily large
char dguid_s[GDP_NAME_HEX_STRING];
char eguid_s[GDP_NAME_HEX_STRING];
char query[GDP_QUERY_STRING];

// optional debug with levels
#if 1
#define ERR   1
#define WARN  2
#define INFO  3
#define VERB  4
int debug_knob = INFO;
#define debug(d, fmt, ...)						 \
	do											 \
	{											 \
		if (d <= debug_knob)					 \
			fprintf(stderr, fmt, ##__VA_ARGS__); \
	} while (0)

#else
#define debug(...)
#endif

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
	int si_rem_len = sizeof(si_rem);
	int fd_listen;
	int on = 1;
	int otw_pdu_len;
	MYSQL *mysql_con;
	MYSQL_RES *mysql_result;
	unsigned int mysql_fields;
	MYSQL_ROW mysql_row;

	// sanity check structure alterations
	assert(sizeof(otw_pdu) == OTW_PDU_SIZE);
	
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
		otw_pdu_len = recvfrom(fd_listen, &otw_pdu.ver, OTW_PDU_SIZE, 0,
							 (struct sockaddr *)&si_rem, &si_rem_len);

		// parse request
		if (otw_pdu_len != OTW_PDU_SIZE ||
			otw_pdu.ver != GDP_CHAN_PROTO_VERSION)
		{
			debug(WARN, "Info: unrecognized packet from %s:%d len %d",
				  inet_ntoa(si_rem.sin_addr),
				  ntohs(si_rem.sin_port), otw_pdu_len);
			continue;
		}
		debug(INFO, "Received packet from %s:%d\n", 
			  inet_ntoa(si_rem.sin_addr),
			  ntohs(si_rem.sin_port));
		
		debug(VERB, "dguid: ");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			debug(VERB, "%.2x", otw_pdu.dst[i]);
			sprintf(dguid_s + (i * 2), "%.2x", otw_pdu.dst[i]);
		}
		debug(VERB, "\n");
		dguid_s[64] = '\0';
		debug(INFO, "dguid is [%s]\n", dguid_s);

		debug(VERB, "dguid: ");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			debug(VERB, "%.2x", otw_pdu.src[i]);
			sprintf(eguid_s + (i * 2), "%.2x", otw_pdu.src[i]);
		}
		debug(VERB, "\n");
		eguid_s[64] = '\0';
		debug(INFO, "eguid is [%s]\n", eguid_s);

		// handle request
		switch (otw_pdu.cmd)
		{
			
		case GDP_CMD_DIR_ADD:
			// one entry per key for now (via replace instead of insert)
			debug(INFO, "cmd -> add\n");

			// replace or create
			sprintf(query, "replace into blackbox.gdpd values (x'%s', x'%s')",
					dguid_s, eguid_s);
			if (mysql_query(mysql_con, query))
			{
				fprintf(stderr, "Error: %s\n", mysql_error(mysql_con));
				fail(mysql_con, query);
			}
			
			// FIXME add ack?
			break;
			
		case GDP_CMD_DIR_LOOKUP:
			// one entry per key, so no multiple replies for now
			debug(INFO, "cmd -> lookup\n");

			// search
			sprintf(query,
					"select eguid from blackbox.gdpd where dguid = x'%s'",
					dguid_s);
			if (mysql_query(mysql_con, query))
			{
				fprintf(stderr, "Error: %s\n", mysql_error(mysql_con));
				fail(mysql_con, query);
			}

			// parse search result
			mysql_result = mysql_store_result(mysql_con);
			if (mysql_result == NULL)
			{
				fprintf(stderr, "Error: %s\n", mysql_error(mysql_con));
				fail(mysql_con, query);
			}
			mysql_fields = mysql_field_count(mysql_con);
			mysql_row = mysql_fetch_row(mysql_result);
			if (!mysql_row || mysql_fields != 1 || mysql_row[0] == NULL)
			{
				debug(INFO, "dguid not found\n");
				memset(&otw_pdu.src[0], 0, sizeof(gdp_name_t));
			}
			else
			{
				debug(INFO, "dguid found\neguid is [");
				for (int c = 0; c < sizeof(gdp_name_t); c++)
				{
					debug(INFO, "%.2x", (uint8_t) mysql_row[0][c]);
					otw_pdu.src[c] = (uint8_t) mysql_row[0][c];
				}
				debug(INFO, "]\n");

				// signal lookup succeeded by changing cmd from LOOKUP to ADD
				otw_pdu.cmd = GDP_CMD_DIR_ADD;
			}
			mysql_free_result(mysql_result);

			otw_pdu_len = sendto(fd_listen, &otw_pdu.ver, OTW_PDU_SIZE, 0,
							   (struct sockaddr *)&si_rem, sizeof(si_rem));
			debug(INFO, "sendto len %d\n", otw_pdu_len);
			if (otw_pdu_len < 0)
				fprintf(stderr, "Error: sendto len %d error %s\n",
						otw_pdu_len, strerror(errno));
			break;
			
		case GDP_CMD_DIR_REMOVE:
			// one entry per key, so remove is simple for now
			debug(INFO, "cmd -> remove\n");

			// query
			sprintf(query, "delete from blackbox.gdpd where dguid = x'%s'",
					dguid_s);
			if (mysql_query(mysql_con, query))
			{
				fprintf(stderr, "Error: %s\n", mysql_error(mysql_con));
				fail(mysql_con, query);
			}
			
			// FIXME add ack?
			break;
			
		default:
			fprintf(stderr, "Error: packet with unknown cmd\n");
			break;
			
		}
	}

	// unreachable at the moment, but leave as a reminder to future expansion
	mysql_close(mysql_con);
	close(fd_listen);
	exit(0);
}
