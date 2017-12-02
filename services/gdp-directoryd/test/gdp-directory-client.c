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
#include <gdp/gdp.h>

// FIXME temporary assignment
#define DIRECTORY_IP "128.32.62.174"
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

gdp_name_t gdp_dguid;
gdp_name_t gdp_eguid;

void help(char *s)
{
	printf("Error: %s\n", s);
	printf("Usage: bb-test { add | lookup | remove } <dguid> [ <eguid> ]\n");
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

	// very simple CLI rather than hyphenated options
	if (argc < 3)
		help("invalid parameter(s)");
	
	if (strcmp(argv[1], "lookup") == 0)
	{
		if (argc > 3)
			help("extraneous parameter(s)");
		if (argc < 3)
			help("software bug");
		otw_pdu.cmd = GDP_CMD_DIR_LOOKUP;
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

	// build the request packet
	otw_pdu.ver = GDP_CHAN_PROTO_VERSION;
	
	gdp_parse_name(argv[2], gdp_dguid);
	if (otw_pdu.cmd != GDP_CMD_DIR_LOOKUP)
		gdp_parse_name(argv[3], gdp_eguid);
	
	memcpy(&otw_pdu.dst[0], gdp_dguid, sizeof(gdp_name_t));

	printf("dguid is [");
	for (int i = 0; i < sizeof(gdp_name_t); i++)
	{
		printf("%.2x", gdp_dguid[i]);
	}
	printf("]\n");

	if (otw_pdu.cmd != GDP_CMD_DIR_LOOKUP)
	{
		memcpy(&otw_pdu.src[0], gdp_eguid, sizeof(gdp_name_t));
		
		printf("eguid is [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			//dgram[MSG_OFFSET_SRC + i] = gdp_eguid[i];
			printf("%.2x", gdp_eguid[i]);
		}
		printf("]\n");
	}
	else
		memset(&otw_pdu.src[0], 0, sizeof(gdp_name_t));

	// connection-oriented udp - hardwired destination
	send(fd_connect, &otw_pdu.ver, OTW_PDU_SIZE, 0);

	if (otw_pdu.cmd == GDP_CMD_DIR_LOOKUP)
	{
		// connection-oriented udp - kernel filters unsolicited packets out
		if (recv(fd_connect, &otw_pdu.ver, OTW_PDU_SIZE, 0) < 0)
			fail("recv");

		if (otw_pdu.cmd == GDP_CMD_DIR_ADD)
		{
			printf("eguid is [");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				printf("%.2x", (uint8_t) otw_pdu.src[i]);
			}
			printf("]\n");
		}
		else
			printf("eguid not found\n");
	}
	
	close(fd_connect);
	return 0;
}
