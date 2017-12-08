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

// debug with levels
#define ERR   1
#define WARN  2
#define INFO  3
#define VERB  4
int debug_knob = WARN;
#define debug(d, fmt, ...)						 \
	do											 \
	{											 \
		if (d <= debug_knob)					 \
			fprintf(stderr, fmt, ##__VA_ARGS__); \
	} while (0)

// production port
#define PORT 9001
// development port
// #define PORT 9002

// FIXME version 3 otw_pdu support for the moment...
#define GDP_CHAN_PROTO_VERSION 3

// FIXME temporary until cmd extensions designed and added to gdp_pdu.h
#define GDP_CMD_DIR_ADD		7
#define GDP_CMD_DIR_REMOVE	8
#define GDP_CMD_DIR_FIND	9
#define GDP_CMD_DIR_FOUND  10

// FIXME eventually maintain this in gdp_chan.h or other appropriate shared .h
typedef struct __attribute((packed)) otw_pdu_v3_s
{
	uint8_t ver;
	uint8_t ttl;
	uint8_t rsvd1;
	uint8_t cmd;
	uint8_t dguid[sizeof(gdp_name_t)];
	uint8_t eguid[sizeof(gdp_name_t)];
	uint8_t cguid[sizeof(gdp_name_t)];
} otw_pdu_v3_t;

// on the wire pdu
#define OTW_PDU_SIZE 100 // sanity check otw_pdu structure size
otw_pdu_v3_t otw_pdu;


