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
#include "../gdp-ribd.h"

// loopback is default (edit to specify remote gdp-ribd ip address)
#define DIRECTORY_IP "127.0.0.1"

gdp_name_t gdp_eguid;
gdp_name_t gdp_dguid;
gdp_name_t gdp_oguid;

otw_dir_t otw_dir;

void help(char *s)
{
	printf("Error: %s\n", s);
	printf("Usage: gdp-nhop <command>\n"
		   "\t{ add | delete } <eguid> <dguid>\n"
		   "\tfind <oguid> <dguid>\n"
		   "\tmfind <oguid> <dguid>\n"
		   "\tflush <eguid>\n");
	exit(1);
}

void fail(char *s)
{
	printf("Error: %s\n", s);
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
	unsigned int rows;

	if (argc < 2)
		help("missing parameter(s)");
	if (strcmp(argv[1], "add") == 0)
	{
		if (argc > 4)
			help("extraneous parameter(s)");			
		if (argc < 4)
			help("missing parameter(s)");
		otw_dir.cmd = GDP_CMD_DIR_ADD;
	}
	else if (strcmp(argv[1], "delete") == 0)
	{
		if (argc > 4)
			help("extraneous parameter(s)");			
		if (argc < 4)
			help("missing parameter(s)");
		otw_dir.cmd = GDP_CMD_DIR_DELETE;
	}
	else if (strcmp(argv[1], "find") == 0)
	{
		if (argc > 4)
			help("extraneous parameter(s)");
		if (argc < 4)
			help("missing parameter(s)");
		otw_dir.cmd = GDP_CMD_DIR_FIND;
	}
	else if (strcmp(argv[1], "mfind") == 0)
	{
		if (argc > 4)
			help("extraneous parameter(s)");
		if (argc < 4)
			help("missing parameter(s)");
		otw_dir.cmd = GDP_CMD_DIR_MFIND;
	}
	else if (strcmp(argv[1], "flush") == 0)
	{
		if (argc > 3)
			help("extraneous parameter(s)");			
		if (argc < 3)
			help("missing parameter(s)");
		otw_dir.cmd = GDP_CMD_DIR_FLUSH;
	}
	else
	{
		help("invalid action");
	}
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
	
	otw_dir.ver = GDP_CHAN_PROTO_VERSION_4;

	fd_dr = open("/dev/urandom", O_RDONLY);
	if (read(fd_dr, &dr, sizeof(dr)) != 2)
	{
		fail("read /dev/urandom error");
	}
	else
	{
		close(fd_dr);
	}
	
	otw_dir.id = htons(dr); // htons comments on expected endianess of id field

	// store first guid parameter in eguid field
	gdp_parse_name(argv[arg_index], gdp_eguid);
	memcpy(&otw_dir.eguid[0][0], gdp_eguid, sizeof(gdp_name_t));
	arg_index++;

	if (otw_dir.cmd != GDP_CMD_DIR_FLUSH)
	{
		// store second guid parameter in dguid field
		gdp_parse_name(argv[arg_index], gdp_dguid);
		memcpy(&otw_dir.dguid[0], gdp_dguid, sizeof(gdp_name_t));
		arg_index++;
	
		printf("-> dguid [");
		for (int i = 0; i < sizeof(gdp_name_t); i++)
		{
			printf("%.2x", otw_dir.dguid[i]);
		}
		printf("]\n");
	}
	else
	{
		// dguid field unused on flush
		memset(&otw_dir.dguid[0], 0, sizeof(gdp_name_t));
	}
	
	printf("-> eguid [");
	for (int i = 0; i < sizeof(gdp_name_t); i++)
	{
		printf("%.2x", otw_dir.eguid[0][i]);
	}
	printf("]\n");

	if (otw_dir.cmd == GDP_CMD_DIR_FLUSH)
	{
		// connection-oriented udp - hardwired testbed destination, simply send
		otw_dir_len = send(fd_connect, (uint8_t *) &otw_dir.ver,
						   offsetof(otw_dir_t, eguid[1]), 0);
		printf("id(0x%x) send len %lu\n", ntohs(otw_dir.id),
			   offsetof(otw_dir_t, eguid[1]));

		// flush does not send a reply
		close(fd_connect);
		return 0;
	}

	// connection-oriented udp - hardwired testbed destination, simply send
	otw_dir_len = send(fd_connect, (uint8_t *) &otw_dir.ver,
					   offsetof(otw_dir_t, eguid[1]), 0);
	printf("id(0x%x) send len %d\n", ntohs(otw_dir.id), otw_dir_len);

	// only find and mfind requests produce a reply
	if (otw_dir.cmd != GDP_CMD_DIR_FIND && otw_dir.cmd != GDP_CMD_DIR_MFIND)
	{
		close(fd_connect);
		return 0;
	}

	printf("...awaiting reply...\n");

	// connection-oriented udp - kernel filters unsolicited packets out, await
	otw_dir_len = recv(fd_connect, (uint8_t *) &otw_dir.ver,
					   sizeof(otw_dir), 0);
	if (otw_dir_len < 0)
	{
		fail("recv");
	}
	if (otw_dir_len < offsetof(otw_dir_t, eguid[0]))
	{
		fail("short");
	}
	if ((otw_dir_len - offsetof(otw_dir_t, eguid[0])) % sizeof(gdp_name_t))
	{
		fail("invalid length");
	}
	printf("id(0x%x) recv len %d\n", ntohs(otw_dir.id), otw_dir_len);
		
	printf("<- dguid [");
	for (int i = 0; i < sizeof(gdp_name_t); i++)
	{
		printf("%.2x", otw_dir.dguid[i]);
	}
	printf("]\n");

	if (otw_dir.cmd == GDP_CMD_DIR_FIND || otw_dir.cmd == GDP_CMD_DIR_MFIND)
	{
		printf("<- eguid nak\n");
	}
	else
	{
		rows = ((otw_dir_len - offsetof(otw_dir_t, eguid[0])) /
				sizeof(gdp_name_t));
		if (otw_dir.cmd == GDP_CMD_DIR_FIND && rows != 1)
		{
			fail("find row count invalid");
		}
		for (int r = 0; r < rows; ++r)
		{
			printf("<- eguid [");
			for (int i = 0; i < sizeof(gdp_name_t); i++)
			{
				printf("%.2x", (uint8_t) otw_dir.eguid[r][i]);
			}
			printf("]\n");
		}
	}
	
	close(fd_connect);
	return 0;
}
