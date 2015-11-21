/*
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**	----- END LICENSE BLOCK -----
*/

#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <event2/event.h>

#define RENDEZVOUS	"./r"

void
read_cb(evutil_socket_t fd, short what, void *arg)
{
	int i;
	char buf[4];

	printf("Data to read on fd = %d\n", fd);
	i = read(fd, buf, sizeof buf);
	if (i < 0)
		perror("read");
	else
		printf("%*s\n", i, buf);
}

int
main(int argc, char **argv)
{
	struct event_base *eb;
	struct event *ev;
	int fd;
	int i;

	// set up the rendezvous file (create if necessary, but we will read)
	fd = open(RENDEZVOUS, O_RDONLY | O_CREAT, 0777);
	if (fd < 0)
	{
		perror(RENDEZVOUS);
		exit(1);
	}

	// we need arbitrary fd access
	{
		struct event_config *ev_cfg = event_config_new();

		event_config_require_features(ev_cfg, EV_FEATURE_FDS);

		// set up the event to read the rendezvous file
		eb = event_base_new_with_config(ev_cfg);
		if (eb == NULL)
		{
			perror("Cannot create event_base");
			exit(1);
		}
		event_config_free(ev_cfg);
	}

	// just for yucks, let's see what it can do
	printf("Base method: %s\n", event_base_get_method(eb));
	i = event_base_get_features(eb);
	if (i & EV_FEATURE_ET)
		printf("  Edge-triggered events\n");
	if (i & EV_FEATURE_O1)
		printf("  O(1) notification\n");
	if (i & EV_FEATURE_FDS)
		printf("  All fd types\n");

	// set up an event
	ev = event_new(eb, fd, EV_READ | EV_PERSIST, read_cb, NULL);
	event_add(ev, NULL);

	// now run the actual loop
	i = event_base_loop(eb, 0);
	printf("event_base_loop => %d\n", i);

	exit(i);
}
