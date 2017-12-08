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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <assert.h>
#include <gdp/gdp.h>
#include "../gdp-directoryd.h"

// gdp directory currently installed on gdp-04
#define DIRECTORY_IP "128.32.62.174"

gdp_name_t gdp_dguid;
gdp_name_t gdp_eguid;
gdp_name_t gdp_cguid;

otw_pdu_dir_t otw_pdu;

void help(char *s)
{
	printf("Error: %s\n", s);
	printf("Usage: bb-test { add | find | remove } <dguid> [ <eguid> ]\n");
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

	// sanity check structure packing
	assert(sizeof(otw_pdu) == OTW_PDU_SIZE);

	// very simple CLI rather than hyphenated options
	if (argc < 3)
		help("invalid parameter(s)");
	
	if (strcmp(argv[1], "find") == 0)
	{
		if (argc > 3)
			help("extraneous parameter(s)");
		if (argc < 3)
			help("software bug");
		otw_pdu.cmd = GDP_CMD_DIR_FIND;
	}
	else if (strcmp(argv[1], "add") == 0)
	{
		if (argc != 4)
			help("invalid or missing parameters");
		otw_pdu.cmd = GDP_CMD_DIR_ADD;
	}
	else if (strcmp(argv[1], "remove") == 0)
	{
		if (argc != 4)
			help("invalid or missing parameters");
		otw_pdu.cmd = GDP_CMD_DIR_REMOVE;
	}
	else
		help("invalid action");

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
	// add/delete advert: dguid (client), eguid (egress router)
	// find: dguid (find), eguid (0 or found), cguid (client)
	//

	otw_pdu.ver = GDP_CHAN_PROTO_VERSION;

	// dguid
	gdp_parse_name(argv[2], gdp_dguid);
	memcpy(&otw_pdu.dguid[0], gdp_dguid, sizeof(gdp_name_t));
	printf("-> dguid [");
	for (int i = 0; i < sizeof(gdp_name_t); i++)
	{
		printf("%.2x", otw_pdu.dguid[i]);
	}
	printf("]\n");

	if (otw_pdu.cmd != GDP_CMD_DIR_FIND)
	{
		// eguid 
		gdp_parse_name(argv[3], gdp_eguid);
	    memcpy(&otw_pdu.eguid[0], gdp_eguid, sizeof(gdp_name_t));
		printf("-> eguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", otw_pdu.eguid[i]);
		}
		printf("]\n");

		// connection-oriented udp - hardwired destination, simply send
		send(fd_connect, (uint8_t *) &otw_pdu.ver,
			 OTW_PDU_SIZE - sizeof(gdp_name_t), 0);
	}
	else
	{
		// find eguid
		memset(&otw_pdu.eguid[0], 0x00, sizeof(gdp_name_t));

		// set (fake) cguid to verify (0xc1c1c1... is) returned by server
		memset(&otw_pdu.cguid[0], 0xC1, sizeof(gdp_name_t));
		printf("-> cguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", (uint8_t) otw_pdu.cguid[i]);
		}
		printf("]\n");

		// connection-oriented udp - hardwired destination, simply send
		send(fd_connect, (uint8_t *) &otw_pdu.ver, OTW_PDU_SIZE, 0);
	}

	// connection-oriented udp - kernel filters unsolicited packets out, await
	if (recv(fd_connect, (uint8_t *) &otw_pdu.ver, OTW_PDU_SIZE, 0) < 0)
		fail("recv");

	if (otw_pdu.cmd != GDP_CMD_DIR_FIND)
	{
		printf("<- dguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", otw_pdu.dguid[i]);
		}
		printf("]\n");
		printf("<- eguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", (uint8_t) otw_pdu.eguid[i]);
		}
		printf("] ack\n");
	}
	else
		printf("<- eguid nak\n");
	
	if (otw_pdu.cmd == GDP_CMD_DIR_FIND ||
		otw_pdu.cmd == GDP_CMD_DIR_FOUND)
	{
		printf("<- cguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", (uint8_t) otw_pdu.cguid[i]);
		}
		printf("]\n");
	}
	
	close(fd_connect);
	return 0;
}
