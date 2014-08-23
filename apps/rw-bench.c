/* vim: set ai sw=4 sts=4 : */

#include <gdp/gdp.h>

#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include <sys/time.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

struct elapsed_time {
	long seconds;
	long millis;
};

void
avg_elapsed_time(struct elapsed_time *total_elapsed_time, size_t n, struct elapsed_time *out) {
	assert(n > 0);
	out->millis = (total_elapsed_time->seconds * 1000) + total_elapsed_time->millis;
	out->millis /= n;
	out->seconds = out->millis / 1000;
	out->millis -= out->seconds * 1000;
}

void
sum_elapsed_time(struct elapsed_time elapsed_time[], size_t n, struct elapsed_time *out) {
	size_t i;
	long new_seconds;
	out->seconds = 0;
	out->millis = 0;
	for (i = 0; i < n; ++i) {
		out->seconds += elapsed_time[i].seconds;
		out->millis += elapsed_time[i].millis;
	}
	new_seconds = out->millis / 1000;
	out->seconds += new_seconds;
	out->millis -= new_seconds * 1000;
}

void
get_elapsed_time(
		EP_TIME_SPEC *start_time, EP_TIME_SPEC *end_time,
		struct elapsed_time *out)
{
	out->millis = ((end_time->tv_sec - start_time->tv_sec) * 1000) +
		((end_time->tv_nsec - start_time->tv_nsec) / (1000 * 1000));
	out->seconds = out->millis / 1000;
	out->millis -= out->seconds * 1000;
}

void
print_elapsed_time(FILE *stream, struct elapsed_time *elapsed_time) {
	fprintf(stream, "Elapsed time = %lu.%03lu s\n", elapsed_time->seconds, elapsed_time->millis);
}

int
random_in_range(unsigned int min, unsigned int max)
{
	int base_random = rand(); /* in [0, RAND_MAX] */
	if (RAND_MAX == base_random)
		return random_in_range(min, max);
	/* now guaranteed to be in [0, RAND_MAX) */
	int range = max - min, remainder = RAND_MAX % range, bucket = RAND_MAX
	        / range;
	/* There are range buckets, plus one smaller interval
	 within remainder of RAND_MAX */
	if (base_random < RAND_MAX - remainder)
	{
		return min + base_random / bucket;
	}
	else
	{
		return random_in_range(min, max);
	}
}

int
main(int argc, char *argv[])
{
	gdp_gcl_t *gclh_write;
	gdp_gcl_t *gclh_read;
	int opt;
	int num_records = 1000;
	int min_length = 1023;
	int max_length = 2047;
	int trials = 1;
	EP_STAT estat;
	char buf[200];

	while ((opt = getopt(argc, argv, "D:n:t:m:M:")) > 0)
	{
		switch (opt)
		{
		case 'D':
			ep_dbg_set(optarg);
			break;
		case 'n':
			num_records = atoi(optarg);
			break;
		case 't':
			trials = atoi(optarg);
			break;
		case 'm':
			min_length = atoi(optarg);
			break;
		case 'M':
			max_length = atoi(optarg);
			break;
		}
	}

	estat = gdp_init(true);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	fprintf(stdout, "\nRunning trials\n\n");

	EP_TIME_SPEC start_time;
	EP_TIME_SPEC end_time;
	struct elapsed_time total_e_time;
	struct elapsed_time avg_e_time;
	struct elapsed_time *trial_write_times;
	struct elapsed_time *trial_read_times;
	char *data;
	size_t record_size;
	size_t max_record_size = max_length + 1;
	size_t data_size = num_records * (max_record_size);
	char *cur_record;
	char *cur_record_b64;
	gdp_datum_t datum;
	gcl_name_t internal_name;
	gcl_pname_t printable_name;
	struct evbuffer *evb = evbuffer_new();

	data = malloc(data_size);
	cur_record = malloc(max_record_size);
	cur_record_b64 = malloc((2 * max_length) + 1);
	trial_write_times = malloc(trials * sizeof(struct elapsed_time));
	trial_read_times = malloc(trials * sizeof(struct elapsed_time));

	int t;
	int i;

	for (t = 0; t < trials; ++t)
	{
		fprintf(stdout, "Trial %d\n", t);
		fprintf(stdout, "Generating %d records of length [%d, %d]\n",
		        num_records, min_length, max_length);
		for (i = 0; i < num_records; ++i)
		{
			evutil_secure_rng_get_bytes(cur_record, max_length);
			ep_b64_encode(cur_record, max_length, cur_record_b64,
			        (2 * max_length) + 1, EP_B64_ENC_URL);
			record_size = random_in_range(min_length, max_length + 1);
			memcpy(data + (i * max_record_size), cur_record_b64, record_size);
			data[(i * max_record_size) + record_size] = '\0';
			//fprintf(stdout, "Msgno = %d\n", i + 1);
			//fprintf(stdout, "%s\n", &data[(i * max_record_size)]);
			//fprintf(stdout, "record length: %lu\n", strlen(&data[(i * max_record_size)]));
		}

		estat = gdp_gcl_create(NULL, NULL, &gclh_write);

		EP_STAT_CHECK(estat, goto fail0);
		gdp_gcl_print(gclh_write, stdout, 0, 0);

		ep_time_now(&start_time);
		fprintf(stdout, "Writing data (start_time = %llu:%u)\n", start_time.tv_sec, start_time.tv_nsec);
		for (i = 0; i < num_records; ++i) {
			memset(&datum, '\0', sizeof datum);
			datum.data = &data[(i * max_record_size)];
			datum.len = strlen(datum.data);
			datum.datumno = i + 1;

			estat = gdp_gcl_append(gclh_write, &datum);
			EP_STAT_CHECK(estat, goto fail1);
		}
		ep_time_now(&end_time);
		fprintf(stdout, "Finished writing data (end_time = %llu:%u)\n", end_time.tv_sec, end_time.tv_nsec);
		get_elapsed_time(&start_time, &end_time, &trial_write_times[t]);
		print_elapsed_time(stdout, &trial_write_times[t]);
		memcpy(internal_name, gdp_gcl_getname(gclh_write), sizeof internal_name);
		gdp_gcl_printable_name(internal_name, printable_name);
		gdp_gcl_close(gclh_write);
		estat = gdp_gcl_open(internal_name, GDP_MODE_RO, &gclh_read);
		ep_time_now(&start_time);
		fprintf(stdout, "Reading data (start_time = %llu:%u)\n", start_time.tv_sec, start_time.tv_nsec);
		for (i = 0; i < num_records; ++i) {
			estat = gdp_gcl_read(gclh_read, i + 1, &datum, evb);
			EP_STAT_CHECK(estat, goto fail2);
			datum.len = evbuffer_remove(evb, cur_record, max_record_size);
			cur_record[datum.len] = '\0';
			datum.data = cur_record;
			if (strncmp(data + (i * max_record_size), datum.data, max_length) != 0) {
				fprintf(stdout, "data mismatch:\n> expected: %s\n> got     : %s\n",
					data + (i * max_record_size), cur_record);
			}

			evbuffer_drain(evb, UINT_MAX);
		}
		ep_time_now(&end_time);
		fprintf(stdout, "Finished reading data (end_time = %llu:%u)\n", end_time.tv_sec, end_time.tv_nsec);
		get_elapsed_time(&start_time, &end_time, &trial_read_times[t]);
		print_elapsed_time(stdout, &trial_read_times[t]);
		fprintf(stdout, "\n");
	}

	sum_elapsed_time(trial_read_times, trials, &total_e_time);
	avg_elapsed_time(&total_e_time, trials, &avg_e_time);

	fprintf(stdout, "Average read time per trial: %lu.%03lu s\n", avg_e_time.seconds, avg_e_time.millis);

	sum_elapsed_time(trial_write_times, trials, &total_e_time);
	avg_elapsed_time(&total_e_time, trials, &avg_e_time);

	fprintf(stdout, "Average write time per trial: %lu.%03lu s\n", avg_e_time.seconds, avg_e_time.millis);

	free(trial_read_times);
	free(trial_write_times);
	free(cur_record_b64);
	free(cur_record);
	free(data);

	goto done;

fail2:
	gdp_gcl_close(gclh_read);

fail1:
	gdp_gcl_close(gclh_write);

fail0:
done:
	fprintf(stderr, "exiting with status %s\n",
	ep_stat_tostr(estat, buf, sizeof buf));

	return !EP_STAT_ISOK(estat);
}
