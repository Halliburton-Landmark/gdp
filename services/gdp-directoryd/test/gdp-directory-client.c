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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <assert.h>
#include "gdp/gdp.h"
#include "../gdp-directoryd.h"

// gdp-04
// #define DIRECTORY_IP "128.32.62.174"
// gdp-03
#define DIRECTORY_IP "128.32.33.232"

gdp_name_t gdp_eguid;
gdp_name_t gdp_dguid;
gdp_name_t gdp_oguid;

otw_dir_t otw_dir;

void help(char *s)
{
	printf("Error: %s\n", s);
	printf("Usage: gdc-test "
		   "{ { add <eguid> <dguid> | find <oguid> <dguid> }\n");
	exit(1);
}

void fail(char *s)
{
	perror(s);
	exit(1);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in si_loc;
	struct sockaddr_in si_rem;
	int fd_connect;
	int arg_index;
	int otw_dir_len;
	int fd_dr;
	uint16_t dr;

	if (strcmp(argv[1], "find") == 0)
	{
		if (argc > 4)
			help("extraneous parameter(s)");
		if (argc < 4)
			help("missing parameter(s)");
		otw_dir.cmd = GDP_CMD_DIR_FIND;
	}
	else if (strcmp(argv[1], "add") == 0)
	{
		if (argc > 4)
			help("extraneous parameter(s)");			
		if (argc < 4)
			help("missing parameter(s)");
		otw_dir.cmd = GDP_CMD_DIR_ADD;
	}
	else
		help("invalid action");
	arg_index = 2;

	// blocking, connection-oriented udp
	if ((fd_connect = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		fail("socket");

	si_loc.sin_family = AF_INET;
	si_loc.sin_addr.s_addr = htonl(INADDR_ANY);
	si_loc.sin_port = 0;
	if (bind(fd_connect, (struct sockaddr *)&si_loc, sizeof(si_loc)) < 0)
		fail("bind src");

	si_rem.sin_family = AF_INET;
	if (inet_aton(DIRECTORY_IP, &si_rem.sin_addr) == 0)
		fail("DIRECTORY_IP invalid");
	si_rem.sin_port = htons(PORT);
	if (connect(fd_connect, (struct sockaddr *)&si_rem, sizeof(si_rem)) < 0)
		fail("connect");

	//
	// build request
	//
	// { add eguid dguid } | find { oguid dguid }
	//
	
	otw_dir.ver = GDP_CHAN_PROTO_VERSION_4;

	fd_dr = open("/dev/urandom", O_RDONLY);
	if (read(fd_dr, &dr, sizeof(dr)) != 2)
		fail("read /dev/urandom error");
	else
		close(fd_dr);
	
	otw_dir.id = htons(dr); // htons comments on expected endianess of id field
	
	if (otw_dir.cmd == GDP_CMD_DIR_ADD)
	{
		// set eguid
		gdp_parse_name(argv[arg_index], gdp_eguid);
	    memcpy(&otw_dir.eguid[0], gdp_eguid, sizeof(gdp_name_t));
		printf("-> eguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", otw_dir.eguid[i]);
		}
		printf("]\n");
		arg_index++;
	}
	else
		memset(&otw_dir.eguid[0], 0x00, sizeof(gdp_name_t));

	// set required dguid
	gdp_parse_name(argv[arg_index], gdp_dguid);
	memcpy(&otw_dir.dguid[0], gdp_dguid, sizeof(gdp_name_t));
	printf("-> dguid [");
	for (int i = 0; i < sizeof(gdp_name_t); i++)
	{
		printf("%.2x", otw_dir.dguid[i]);
	}
	printf("]\n");
	arg_index++;

	if (otw_dir.cmd == GDP_CMD_DIR_FIND)
	{
		// set required oguid
		gdp_parse_name(argv[arg_index], gdp_oguid);
		memcpy(&otw_dir.oguid[0], gdp_oguid, sizeof(gdp_name_t));
		printf("-> oguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", otw_dir.oguid[i]);
		}
		printf("]\n");
		arg_index++;
		
		// connection-oriented udp - hardwired destination, simply send
		otw_dir_len = send(fd_connect, (uint8_t *) &otw_dir.ver,
						   sizeof(otw_dir_t), 0);
		printf("id(0x%x) send len %d\n", ntohs(otw_dir.id), otw_dir_len);
	}
	else
	{
		// connection-oriented udp - hardwired destination, simply send
		otw_dir_len = send(fd_connect, (uint8_t *) &otw_dir.ver,
						   offsetof(otw_dir_t, oguid), 0);
		printf("id(0x%x) send len %d\n", ntohs(otw_dir.id), otw_dir_len);
	}

	printf("...awaiting reply...\n");

	// connection-oriented udp - kernel filters unsolicited packets out, await
	otw_dir_len = recv(fd_connect, (uint8_t *) &otw_dir.ver,
					   sizeof(otw_dir), 0);
	if (otw_dir_len < 0)
		fail("recv");

	if (otw_dir_len < offsetof(otw_dir_t, oguid))
		fail("short");

	printf("id(0x%x) recv len %d\n", ntohs(otw_dir.id), otw_dir_len);
		
	if (otw_dir.cmd == GDP_CMD_DIR_FIND)
	{
		printf("<- eguid nak\n");
	}
	else // FOUND or ADD
	{
		printf("<- eguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", (uint8_t) otw_dir.eguid[i]);
		}
		printf("]\n");
	}

	printf("<- dguid [");
	for (int i = 0; i < sizeof(gdp_name_t); i++)
	{
		printf("%.2x", otw_dir.dguid[i]);
	}
	printf("]\n");

	if (otw_dir.cmd == GDP_CMD_DIR_FOUND || otw_dir.cmd == GDP_CMD_DIR_FIND)
	{
		printf("<- oguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", otw_dir.oguid[i]);
		}
		printf("]\n");
	}
	
	close(fd_connect);
	return 0;
}
