/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  ----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
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
**  ----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <locale.h>
#include <ctype.h>

#include <time.h>

#include "gdp_zc_client.h"

static EP_DBG	DemoMode = EP_DBG_INIT("_demo", "Demo Mode");

typedef enum {
	COMMAND_HELP,
	COMMAND_VERSION,
	COMMAND_BROWSE_SERVICES,
	COMMAND_BROWSE_ALL_SERVICES,
	COMMAND_BROWSE_DOMAINS
#if defined(HAVE_GDBM) || defined(HAVE_DBM)
	, COMMAND_DUMP_STDB
#endif
} Command;

typedef struct Config {
	int verbose;
	int terminate_on_all_for_now;
	int terminate_on_cache_exhausted;
	char *domain;
	char *stype;
	int ignore_local;
	Command command;
	int resolve;
	int no_fail;
	int parsable;
#if defined(HAVE_GDBM) || defined(HAVE_DBM)
	int no_db_lookup;
#endif
} Config;

typedef struct ServiceInfo ServiceInfo;

struct ServiceInfo {
	AvahiIfIndex interface;
	AvahiProtocol protocol;
	char *name, *type, *domain;

	AvahiServiceResolver *resolver;
	Config *config;

	AVAHI_LLIST_FIELDS(ServiceInfo, info);
};

static AvahiSimplePoll *SimplePoll = NULL;
static AvahiClient *Client = NULL;
static int NAllForNow = 0, NCacheExhausted = 0, NResolving = 0;
static AvahiStringList *BrowsedTypes = NULL;
static ServiceInfo *Services = NULL;
static int NColumns = 80;
static int Browsing = 0;
static ZCInfo **InfoList = NULL;
#define MAX_PORT_LEN 5
#define MAX_ADDR_LEN 50

static int infolist_append_front(ZCInfo **list, ZCInfo *info) {
	info->info_next = *list;
	*list = info;
	return 0;
}

static char *make_printable(const char *from, char *to) {
	const char *f;
	char *t;

	for (f = from, t = to; *f; f++, t++)
		*t = isprint(*f) ? *f : '_';

	*t = 0;

	return to;
}


/*
**	Print something about zeroconf.  It appears to be only for debugging.
*/

static void
print_service_line(Config *config,
		char c,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		const char *name,
		const char *type,
		const char *domain,
		int nl)
{
	char ifname[IF_NAMESIZE];

#if defined(HAVE_GDBM) || defined(HAVE_DBM)
	if (!config->no_db_lookup)
		type = stdb_lookup(type);
#endif

	if (config->parsable) {
		char sn[AVAHI_DOMAIN_NAME_MAX], *e = sn;
		size_t l = sizeof(sn);

		printf("%c;%s;%s;%s;%s;%s%s",
			   c,
			   interface != AVAHI_IF_UNSPEC ? if_indextoname(interface, ifname) : "n/a",
			   protocol != AVAHI_PROTO_UNSPEC ? avahi_proto_to_string(protocol) : "n/a",
			   avahi_escape_label(name, strlen(name), &e, &l), type, domain, nl ? "\n" : "");

	} else {
		char label[AVAHI_LABEL_MAX];
		make_printable(name, label);

		printf("%c %6s %4s %-*s %-20s %s\n",
			   c,
			   interface != AVAHI_IF_UNSPEC ? if_indextoname(interface, ifname) : "n/a",
			   protocol != AVAHI_PROTO_UNSPEC ? avahi_proto_to_string(protocol) : "n/a",
			   NColumns-35, label, type, domain);
	}

	fflush(stdout);
}

static void remove_service(Config *c, ServiceInfo *i) {
	assert(c);
	assert(i);

	AVAHI_LLIST_REMOVE(ServiceInfo, info, Services, i);

	if (i->resolver)
		avahi_service_resolver_free(i->resolver);

	avahi_free(i->name);
	avahi_free(i->type);
	avahi_free(i->domain);
	avahi_free(i);
}


static void check_terminate(Config *c) {

	assert(NAllForNow >= 0);
	assert(NCacheExhausted >= 0);
	assert(NResolving >= 0);

	if (NAllForNow <= 0 && NResolving <= 0) {

		if (c->verbose && !c->parsable) {
			printf(": All for now\n");
			NAllForNow++; /* Make sure that this event is not repeated */
		}

		if (c->terminate_on_all_for_now) {
			avahi_simple_poll_quit(SimplePoll);
		}
	}

	if (NCacheExhausted <= 0 && NResolving <= 0) {

		if (c->verbose && !c->parsable) {
			printf(": Cache exhausted\n");
			NCacheExhausted++; /* Make sure that this event is not repeated */
		}

		if (c->terminate_on_cache_exhausted) {
			avahi_simple_poll_quit(SimplePoll);
		}
	}
}

static void service_resolver_callback(
	AvahiServiceResolver *r,
	AvahiIfIndex interface,
	AvahiProtocol protocol,
	AvahiResolverEvent event,
	const char *name,
	const char *type,
	const char *domain,
	const char *host_name,
	const AvahiAddress *a,
	uint16_t port,
	AvahiStringList *txt,
	AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
	void *userdata) {

	ServiceInfo *i = userdata;

	assert(r);
	assert(i);

	switch (event) {
		case AVAHI_RESOLVER_FOUND: {
			char address[AVAHI_ADDRESS_STR_MAX], *t;
			ZCInfo *info = malloc(sizeof(ZCInfo));

			avahi_address_snprint(address, sizeof(address), a);

			t = avahi_string_list_to_string(txt);

			if (i->config->verbose)
			{
				print_service_line(i->config, '=', interface, protocol,
						name, type, domain, 0);

				if (i->config->parsable)
					printf(";%s;%s;%u;%s\n",
						   host_name,
						   address,
						   port,
						   t);
				else
					printf("   hostname = [%s]\n"
						   "   address = [%s]\n"
						   "   port = [%u]\n"
						   "   txt = [%s]\n",
						   host_name,
						   address,
						   port,
						   t);
			}

			// append zcinfo here
			info->port = port;
			info->address = avahi_strdup(address);
			infolist_append_front(InfoList, info);
			avahi_free(t);
			break;
		}

		case AVAHI_RESOLVER_FAILURE:

			fprintf(stderr,
					"Failed to resolve service '%s' of type '%s' in domain '%s': %s\n",
					name, type, domain,
					avahi_strerror(avahi_client_errno(Client)));
			break;
	}


	avahi_service_resolver_free(i->resolver);
	i->resolver = NULL;

	assert(NResolving > 0);
	NResolving--;
	check_terminate(i->config);
	fflush(stdout);
}

static ServiceInfo *add_service(Config *c, AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain) {
	ServiceInfo *i;

	i = avahi_new(ServiceInfo, 1);

	if (c->resolve) {
		if (!(i->resolver = avahi_service_resolver_new(Client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, service_resolver_callback, i))) {
			avahi_free(i);
			fprintf(stderr, "Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(Client)));
			return NULL;
		}

		NResolving++;
	} else
		i->resolver = NULL;

	i->interface = interface;
	i->protocol = protocol;
	i->name = avahi_strdup(name);
	i->type = avahi_strdup(type);
	i->domain = avahi_strdup(domain);
	i->config = c;

	AVAHI_LLIST_PREPEND(ServiceInfo, info, Services, i);

	return i;
}

static ServiceInfo *find_service(AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain) {
	ServiceInfo *i;

	for (i = Services; i; i = i->info_next)
		if (i->interface == interface &&
			i->protocol == protocol &&
			strcasecmp(i->name, name) == 0 &&
			avahi_domain_equal(i->type, type) &&
			avahi_domain_equal(i->domain, domain))

			return i;

	return NULL;
}

static void service_browser_callback(
	AvahiServiceBrowser *b,
	AvahiIfIndex interface,
	AvahiProtocol protocol,
	AvahiBrowserEvent event,
	const char *name,
	const char *type,
	const char *domain,
	AvahiLookupResultFlags flags,
	void *userdata) {

	Config *c = userdata;

	assert(b);
	assert(c);

	switch (event) {
		case AVAHI_BROWSER_NEW: {
			if (c->ignore_local && (flags & AVAHI_LOOKUP_RESULT_LOCAL))
				break;

			if (find_service(interface, protocol, name, type, domain))
				return;

			add_service(c, interface, protocol, name, type, domain);

			if (c->verbose)
				print_service_line(c, '+', interface, protocol, name,
						type, domain, 1);
			break;

		}

		case AVAHI_BROWSER_REMOVE: {
			ServiceInfo *info;

			if (!(info = find_service(interface, protocol, name, type, domain)))
				return;

			remove_service(c, info);

			if (c->verbose)
				print_service_line(c, '-', interface, protocol, name,
						type, domain, 1);
			break;
		}

		case AVAHI_BROWSER_FAILURE:
			fprintf(stderr, "service_browser failed: %s\n", avahi_strerror(avahi_client_errno(Client)));
			avahi_simple_poll_quit(SimplePoll);
			break;

		case AVAHI_BROWSER_CACHE_EXHAUSTED:
			NCacheExhausted --;
			check_terminate(c);
			break;

		case AVAHI_BROWSER_ALL_FOR_NOW:
			NAllForNow --;
			check_terminate(c);
			break;
	}
}

static void browse_service_type(Config *c, const char *stype, const char *domain) {
	AvahiServiceBrowser *b;
	AvahiStringList *i;

	assert(c);
	assert(Client);
	assert(stype);

	for (i = BrowsedTypes; i; i = i->next)
		if (avahi_domain_equal(stype, (char*) i->text))
			return;

	if (!(b = avahi_service_browser_new(
			  Client,
			  AVAHI_IF_UNSPEC,
			  AVAHI_PROTO_UNSPEC,
			  stype,
			  domain,
			  0,
			  service_browser_callback,
			  c))) {

		fprintf(stderr, "avahi_service_browser_new() failed: %s\n", avahi_strerror(avahi_client_errno(Client)));
		avahi_simple_poll_quit(SimplePoll);
	}

	BrowsedTypes = avahi_string_list_add(BrowsedTypes, stype);

	NAllForNow++;
	NCacheExhausted++;
}

static int start(Config *config) {

	assert(!Browsing);

	const char *version, *hn;

	if (!(version = avahi_client_get_version_string(Client))) {
		fprintf(stderr, "Failed to query version string: %s\n", avahi_strerror(avahi_client_errno(Client)));
		return -1;
	}

	if (!(hn = avahi_client_get_host_name_fqdn(Client))) {
		fprintf(stderr, "Failed to query host name: %s\n", avahi_strerror(avahi_client_errno(Client)));
		return -1;
	}

	browse_service_type(config, config->stype, config->domain);

	Browsing = 1;
	return 0;
}

static int create_new_simple_poll_client(Config *config);

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata)
{
	Config *config = userdata;

	/* This function might be called when avahi_client_new() has not
	 * returned yet.*/
	Client = c;

	switch (state) {
		case AVAHI_CLIENT_FAILURE:

			if (config->no_fail && avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED) {
				int error;

				/* We have been disconnected, so let reconnect */

				fprintf(stderr, "Disconnected, reconnecting ...\n");

				avahi_client_free(Client);
				Client = NULL;

				avahi_string_list_free(BrowsedTypes);
				BrowsedTypes = NULL;

				while (Services)
					remove_service(config, Services);

				Browsing = 0;

				error = create_new_simple_poll_client(config);
			} else {
				fprintf(stderr, "Client failure, exiting: %s\n", avahi_strerror(avahi_client_errno(c)));
				avahi_simple_poll_quit(SimplePoll);
			}

			break;

		case AVAHI_CLIENT_S_REGISTERING:
		case AVAHI_CLIENT_S_RUNNING:
		case AVAHI_CLIENT_S_COLLISION:

			if (!Browsing)
				if (start(config) < 0) {
					avahi_simple_poll_quit(SimplePoll);
				}

			break;

		case AVAHI_CLIENT_CONNECTING:

			if (config->verbose && !config->parsable)
				fprintf(stderr, "Waiting for daemon ...\n");

			break;
	}
}

static int init_config(Config *c) {
	assert(c);

	c->terminate_on_cache_exhausted = 0;
	c->terminate_on_all_for_now = 0;
	c->ignore_local = 0;
	c->no_fail = 0;
	c->parsable = 0;
	c->verbose = ep_dbg_test(DemoMode, 1);
	c->resolve = 1;
	c->terminate_on_all_for_now = 1;

	c->stype = avahi_strdup(ep_adm_getstrparam("swarm.gdp.zeroconf.proto",
				"_gdp._tcp"));
	c->domain = avahi_strdup(ep_adm_getstrparam("swarm.gdp.zeroconf.domain",
				"local"));

	return 0;
}

static int
create_new_simple_poll_client(Config *config)
{
	int error = 0;

	Client = avahi_client_new(avahi_simple_poll_get(SimplePoll),
			config->no_fail ? AVAHI_CLIENT_NO_FAIL : 0,
			client_callback,
			config,
			&error);
	if (Client == NULL)
	{
		if (config->verbose)
			fprintf(stderr, "Failed to create client object: %s\n",
					avahi_strerror(error));
		avahi_simple_poll_quit(SimplePoll);
	}
	return error;
}

/////////////////////// Public API below ///////////////////////


/**
 *
 */
int gdp_zc_scan() {
	int error;
	Config config;
	
	init_config(&config);

	if (!(SimplePoll = avahi_simple_poll_new())) {
		fprintf(stderr, "Failed to create simple poll object.\n");
		goto fail0;
	}

	error = create_new_simple_poll_client(&config);
	if (error != 0)
		goto fail1;


	InfoList = (ZCInfo**) malloc(sizeof(ZCInfo*));
	*InfoList = NULL;
	avahi_simple_poll_loop(SimplePoll);
	return 1;

fail1:
	avahi_simple_poll_free(SimplePoll);
	SimplePoll = NULL;
fail0:
	// stub
	return 0;
}

/**
 * reversing isn't on purpose, just the easiest way to copy
 */
static ZCInfo **list_copy_reverse(ZCInfo **list) {
	ZCInfo *i, *k, **newlist;

	newlist = (ZCInfo**) malloc(sizeof(ZCInfo*));
	*newlist = NULL;
	for (i = *list, k = *newlist; i; i = i->info_next) {
		k = malloc(sizeof(ZCInfo));
		k->port = i->port;
		k->address = strndup(i->address, strlen(i->address)+1);
		infolist_append_front(newlist, k);
	}
	return newlist;
}

int list_length(ZCInfo **list) {
	ZCInfo *i;
	int count;

	for (i = *list, count = 0; i; i = i->info_next)
		count++;

	return count;
}

/**
 * zero indexed linked list pop
 * format is "addr:port;"
 * notice the colon and semicolon
 */
static char *list_pop_str(ZCInfo **list, int index) {
	ZCInfo *info, *tmpinfo;
	char *outstr;
	int length, total_strlen, i;

	length = list_length(list);
	total_strlen = length * ((MAX_PORT_LEN+2) + MAX_ADDR_LEN) + 1;
	outstr = malloc(sizeof(char) * total_strlen);
	*outstr = '\0';
	info = *list;
	if (length == 0) {
		free(outstr);
		return NULL;
	} else {
		if (index == 0) { // Removing the "head" node
			snprintf(outstr, total_strlen, "%s:%u;", info->address, info->port);
			if (length != 1) {
				*list = info->info_next;
			}
			// free old head
			free(info->address);
			free(info);
		} else {
			// reach the node right before the node to remove
			for (i = 0; i < index-1; i++) {
				info = info->info_next;
			}
			snprintf(outstr, total_strlen, "%s:%u;",
					info->info_next->address,
					info->info_next->port);
			// remove next node from list
			tmpinfo = info;
			info = info->info_next;
			tmpinfo->info_next = tmpinfo->info_next->info_next;
			// free node
			free(info->address);
			free(info);
		}
	}
	return outstr;
}

char *gdp_zc_addr_str(ZCInfo **list) {
	ZCInfo **listcopy;
	char *outstr, *tmpstr;
	int length, total_strlen, randnum;

	listcopy = list_copy_reverse(list);
	length = list_length(listcopy);
	total_strlen = length * (MAX_PORT_LEN + MAX_ADDR_LEN) + 1;
	outstr = malloc(sizeof(char) * total_strlen);
	*outstr = '\0';
	srand(time(NULL));
	while(length > 0) {
		randnum = rand()%length;
		tmpstr = list_pop_str(listcopy, randnum);
		strlcat(outstr, tmpstr, total_strlen);
		free(tmpstr);
		length--;
	}
	free(listcopy);
	// if len > 0 then remove last char which will be ';'
	if (strlen(outstr) > 0) {
		outstr[strlen(outstr)-1] = '\0';
	}
	return outstr;
}

ZCInfo **gdp_zc_get_infolist() {
	return InfoList;
}

int gdp_zc_free_infolist(ZCInfo **list) {
	ZCInfo *i, *next;
	for (i = *list; i;) {
		free(i->address);
		next = i->info_next;
		free(i);
		i = next;
	}
	free(list);
	list = NULL;
	return 1;
}

/* vim: set noexpandtab : */
