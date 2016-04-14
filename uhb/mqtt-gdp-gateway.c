/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_string.h>

#include <mosquitto.h>

#include <errno.h>
#include <getopt.h>
#include <sysexits.h>
#include <sys/stat.h>


static EP_DBG	Dbg = EP_DBG_INIT("mqtt-gdp-gateway", "MTQQ to GDP gateway");


/*
**  Called by Mosquitto to do logging
*/

void
log_cb(struct mosquitto *mosq,
				void *udata,
				int level,
				const char *str)
{
	const char *level_str = NULL;

	switch (level)
	{
	case MOSQ_LOG_DEBUG:
		//level_str = "debug";
		break;

	case MOSQ_LOG_INFO:
		level_str = "info";
		break;

	case MOSQ_LOG_WARNING:
		level_str = "warning";
		break;

	case MOSQ_LOG_ERR:
		level_str = "error";
		break;

	case MOSQ_LOG_NOTICE:
		level_str = "notice";
		break;

	default:
		fprintf(stderr, "unknown log level %d\n", level);
		level_str = "???";
		break;
	}
	if (level_str != NULL)
		fprintf(stderr, "MOSQ(%s): %s\n", level_str, str);
}


/*
**  GET_TOPIC_INFO --- get information about a given topic
**
**		This implementation is pretty bad.
*/

struct topic_info
{
	const char	*topic_pat;			// pattern for topic
	gdp_gcl_t	*gcl;				// associated GCL
};

struct topic_info	**Topics;		// information about topics
int					NTopics = 0;	// number of topics allocated

struct topic_info *
get_topic_info(const struct mosquitto_message *msg)
{
	int tno;

	ep_dbg_cprintf(Dbg, 2, "get_topic_info(%s): ", msg->topic);
	for (tno = 0; tno < NTopics; tno++)
	{
		struct topic_info *t = Topics[tno];
		if (t == NULL)
			continue;
		bool res;
		int istat = mosquitto_topic_matches_sub(t->topic_pat, msg->topic, &res);
		if (istat != 0 || !res)
			continue;

		// topic matches
		ep_dbg_cprintf(Dbg, 2, "%p (%s)\n", t, t->topic_pat);
		return t;
	}

	// no match
	ep_dbg_cprintf(Dbg, 2, "not found\n");
	return NULL;
}


void
add_topic_info(const char *topic_pat, gdp_gcl_t *gcl)
{
	struct topic_info *t;

	t = ep_mem_zalloc(sizeof *t);
	t->topic_pat = topic_pat;
	t->gcl = gcl;

	Topics = ep_mem_realloc(Topics, (NTopics + 1) * sizeof t);
	Topics[NTopics++] = t;
	ep_dbg_cprintf(Dbg, 3, "add_topic_info(%s): %p\n", topic_pat, t);
}


int
subscribe_to_all_topics(struct mosquitto *mosq, int qos)
{
	int tno;

	for (tno = 0; tno < NTopics; tno++)
	{
		int istat;
		int mid;				// message id --- do we need this?
		struct topic_info *t = Topics[tno];

		if (t == NULL)
			continue;
		ep_dbg_cprintf(Dbg, 5, "subscribe_to_all_topics(%s)\n", t->topic_pat);
		istat = mosquitto_subscribe(mosq, &mid, t->topic_pat, qos);
		if (istat != 0)
			return istat;
	}
	return 0;
}


/*
**  Called by Mosquitto when a message comes in.
*/

void
message_cb(struct mosquitto *mosq,
				void *udata,
				const struct mosquitto_message *msg)
{
	struct topic_info *tinfo;
	const char *payload;

	ep_dbg_cprintf(Dbg, 5, "message_cb: %s @%d => %s\n",
			msg->topic, msg->qos, msg->payload);

	if (msg->payloadlen <= 0)
		payload = "null";
	else
		payload = msg->payload;

	tinfo = get_topic_info(msg);
	if (tinfo == NULL)
	{
		// no place to log this message
		return;
	}

	if (tinfo->gcl != NULL)
	{
		EP_STAT estat;
		gdp_datum_t *datum = gdp_datum_new();

		gdp_buf_printf(gdp_datum_getbuf(datum),
				"{topic:\"%s\", qos:%d, len:%d, payload:%s}\n",
				msg->topic, msg->qos, msg->payloadlen, payload);
		estat = gdp_gcl_append(tinfo->gcl, datum);
		if (!EP_STAT_ISOK(estat))
		{
			ep_log(estat, "cannot log MQTT message");
		}
		gdp_datum_free(datum);
	}
	else
	{
		printf("{topic:\"%s\", qos:%d, len:%d, payload:%s}\n",
				msg->topic, msg->qos, msg->payloadlen, payload);
	}
}


/*
**  Run Mosquitto against a designated broker.
*/

EP_STAT
mosquitto_run(const char *mqtt_broker,
			int subscr_qos,
			void *udata)
{
	struct mosquitto *mosq;
	const char *phase;
	int istat;

	mosquitto_lib_init();

	// get a mosquitto context
	mosq = mosquitto_new(NULL, true, udata);
	if (mosq == NULL)
		goto fail1;

	// can't just pass in NULL for udata, since mosquitto_new automatically
	// maps NULL to mosq
	mosquitto_user_data_set(mosq, udata);

	// set up callbacks
	mosquitto_log_callback_set(mosq, &log_cb);
	mosquitto_message_callback_set(mosq, &message_cb);

	// connect to the broker
	{
		phase = "connect";

		// parse the broker information
		int mqtt_port = 1883;
		char *broker = ep_mem_strdup(mqtt_broker);
		char *p = strrchr(broker, ':');
		if (p != NULL && strchr(p, ']') == NULL)
		{
			*p++ = '\0';
			mqtt_port = atol(p);
		}
		istat = mosquitto_connect(mosq, broker, mqtt_port, 60);
		ep_mem_free(broker);
		if (istat != 0)
			goto fail2;
	}

	phase = "subscribe";
	istat = subscribe_to_all_topics(mosq, subscr_qos);
	if (istat != 0)
		goto fail2;

	phase = "loop";
	istat = mosquitto_loop_forever(mosq, -1, 1);

	// should never get here

fail2:
	if (istat == MOSQ_ERR_ERRNO)
		fprintf(stderr, "mosquitto error in %s: %s\n",
				phase, strerror(errno));
	else
		fprintf(stderr, "mosquitto error in %s: %s\n",
				phase, mosquitto_strerror(istat));
	(void) mosquitto_disconnect(mosq);	// ignore possible MOSQ_ERR_NO_CONN
										// "not connected" error
	mosquitto_destroy(mosq);
	return EP_STAT_ABORT;

fail1:
	fprintf(stderr, "cannot create mosquitto context: %s\n",
			strerror(errno));
	return EP_STAT_ABORT;
}


gdp_gcl_t *
open_signed_log(const char *gdp_log_name, const char *signing_key_file)
{
	EP_STAT estat;
	gdp_name_t gname;
	gdp_gcl_open_info_t *info = NULL;
	gdp_gcl_t *gcl = NULL;

	// make sure we can parse the log name
	estat = gdp_parse_name(gdp_log_name, gname);
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[60];

		ep_app_error("cannot parse log name %s%s%s: %s",
				EpChar->lquote, gdp_log_name, EpChar->rquote,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		goto fail0;
	}

	if (signing_key_file != NULL)
	{
		FILE *fp;
		EP_CRYPTO_KEY *skey;
		struct stat st;

		// set up any open information
		info = gdp_gcl_open_info_new();

		if (stat(signing_key_file, &st) == 0 &&
				(st.st_mode & S_IFMT) == S_IFDIR)
		{
			size_t sz;
			char *p;
			gdp_pname_t pname;

			// we need the printable name for getting the key file name
			gdp_printable_name(gname, pname);

			// find the file in the directory
			sz = strlen(signing_key_file) + sizeof pname + 6;
			p = alloca(sz);
			snprintf(p, sz, "%s/%s.pem", signing_key_file, pname);
			signing_key_file = p;
		}

		fp = fopen(signing_key_file, "r");
		if (fp == NULL)
		{
			ep_app_error("cannot open signing key file %s", signing_key_file);
			goto fail1;
		}

		skey = ep_crypto_key_read_fp(fp, signing_key_file,
				EP_CRYPTO_KEYFORM_PEM, EP_CRYPTO_F_SECRET);
		if (skey == NULL)
		{
			ep_app_error("cannot read signing key file %s", signing_key_file);
			goto fail1;
		}

		estat = gdp_gcl_open_info_set_signing_key(info, skey);
		if (!EP_STAT_ISOK(estat))
		{
fail1:
			if (fp != NULL)
				fclose(fp);
			goto fail0;
		}
	}

	// open a GCL with the provided name
	estat = gdp_gcl_open(gname, GDP_MODE_AO, info, &gcl);
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[60];

		ep_app_error("cannot open log %s%s%s: %s",
				EpChar->lquote, gdp_log_name, EpChar->rquote,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		goto fail0;
	}

fail0:
	if (info != NULL)
		gdp_gcl_open_info_free(info);
	return gcl;
}


void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbgspec] [-G router_addr] [-K signing_key_file]\n"
			"\t[-M broker_addr] [-q qos] mqtt_topic gdp_log\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for gdp_router\n"
			"    -K  key file to use to sign GDP log writes\n"
			"    -M  IP host to contact for MQTT broker\n"
			"    -q  MQTT Quality of Service to request\n"
			"",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	EP_STAT estat;
	bool show_usage = false;
	const char *mqtt_broker = NULL;
	int mqtt_qos = 2;
	char *gdp_addr = NULL;
	char *signing_key_file = NULL;
	int opt;
	char ebuf[60];

	while ((opt = getopt(argc, argv, "D:G:K:M:q:")) > 0)
	{
		switch (opt)
		{
		case 'D':
			ep_dbg_set(optarg);
			break;

		case 'G':
			gdp_addr = optarg;
			break;

		case 'K':
			signing_key_file = optarg;
			break;

		case 'M':
			mqtt_broker = optarg;
			break;

		case 'q':
			mqtt_qos = atoi(optarg);
			break;

		default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage)
		usage();
	if (argc < 2)
	{
		fprintf(stderr, "missing required argument (argc = %d)\n", argc);
		usage();
	}

	// initialize GDP library
	estat = gdp_init(gdp_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed: %s",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		exit(EX_UNAVAILABLE);
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	// open any logs we may use
	for (; argc > 1; argv += 2, argc -= 2)
	{
		struct topic_info *t = ep_mem_zalloc(sizeof *t);
		char *mqtt_topic = argv[0];
		char *gdp_log_name = argv[1];
		gdp_gcl_t *gcl = NULL;

		if (strcmp(gdp_log_name, "-") != 0)
		{
			gcl = open_signed_log(gdp_log_name, signing_key_file);
			if (gcl == NULL)
			{
				// message already given
				show_usage = true;
				continue;
			}
		}

		add_topic_info(mqtt_topic, gcl);
	}

	if (show_usage)
		usage();

	if (mqtt_broker == NULL)
	{
		mqtt_broker = ep_adm_getstrparam("swarm.mqtt-gdp-gateway.broker",
							"127.0.0.1");
	}

	mosquitto_run(mqtt_broker, mqtt_qos, NULL);

	ep_log(estat, "mosquitto_run returned");
	exit(EX_SOFTWARE);
}
