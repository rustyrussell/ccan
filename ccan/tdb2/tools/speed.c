/* Simple speed test for TDB */
#include <err.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "tdb2.h"

/* Nanoseconds per operation */
static size_t normalize(const struct timeval *start,
			const struct timeval *stop,
			unsigned int num)
{
	struct timeval diff;

	timersub(stop, start, &diff);

	/* Floating point is more accurate here. */
	return (double)(diff.tv_sec * 1000000 + diff.tv_usec)
		/ num * 1000;
}

static size_t file_size(void)
{
	struct stat st;

	if (stat("/tmp/speed.tdb", &st) != 0)
		return -1;
	return st.st_size;
}

static int count_record(struct tdb_context *tdb,
			TDB_DATA key, TDB_DATA data, void *p)
{
	int *total = p;
	*total += *(int *)data.dptr;
	return 0;
}

static void dump_and_clear_stats(struct tdb_context **tdb,
				 int flags,
				 union tdb_attribute *attr)
{
	union tdb_attribute stats;
	enum TDB_ERROR ecode;

	stats.base.attr = TDB_ATTRIBUTE_STATS;
	stats.stats.size = sizeof(stats.stats);
	ecode = tdb_get_attribute(*tdb, &stats);
	if (ecode != TDB_SUCCESS)
		errx(1, "Getting stats: %s", tdb_errorstr(ecode));

	printf("allocs = %llu\n",
	       (unsigned long long)stats.stats.allocs);
	printf("  alloc_subhash = %llu\n",
	       (unsigned long long)stats.stats.alloc_subhash);
	printf("  alloc_chain = %llu\n",
	       (unsigned long long)stats.stats.alloc_chain);
	printf("  alloc_bucket_exact = %llu\n",
	       (unsigned long long)stats.stats.alloc_bucket_exact);
	printf("  alloc_bucket_max = %llu\n",
	       (unsigned long long)stats.stats.alloc_bucket_max);
	printf("  alloc_leftover = %llu\n",
	       (unsigned long long)stats.stats.alloc_leftover);
	printf("  alloc_coalesce_tried = %llu\n",
	       (unsigned long long)stats.stats.alloc_coalesce_tried);
	printf("    alloc_coalesce_iterate_clash = %llu\n",
	       (unsigned long long)stats.stats.alloc_coalesce_iterate_clash);
	printf("    alloc_coalesce_lockfail = %llu\n",
	       (unsigned long long)stats.stats.alloc_coalesce_lockfail);
	printf("    alloc_coalesce_race = %llu\n",
	       (unsigned long long)stats.stats.alloc_coalesce_race);
	printf("    alloc_coalesce_succeeded = %llu\n",
	       (unsigned long long)stats.stats.alloc_coalesce_succeeded);
	printf("      alloc_coalesce_num_merged = %llu\n",
	       (unsigned long long)stats.stats.alloc_coalesce_num_merged);
	printf("compares = %llu\n",
	       (unsigned long long)stats.stats.compares);
	printf("  compare_wrong_bucket = %llu\n",
	       (unsigned long long)stats.stats.compare_wrong_bucket);
	printf("  compare_wrong_offsetbits = %llu\n",
	       (unsigned long long)stats.stats.compare_wrong_offsetbits);
	printf("  compare_wrong_keylen = %llu\n",
	       (unsigned long long)stats.stats.compare_wrong_keylen);
	printf("  compare_wrong_rechash = %llu\n",
	       (unsigned long long)stats.stats.compare_wrong_rechash);
	printf("  compare_wrong_keycmp = %llu\n",
	       (unsigned long long)stats.stats.compare_wrong_keycmp);
	printf("transactions = %llu\n",
	       (unsigned long long)stats.stats.transactions);
	printf("  transaction_cancel = %llu\n",
	       (unsigned long long)stats.stats.transaction_cancel);
	printf("  transaction_nest = %llu\n",
	       (unsigned long long)stats.stats.transaction_nest);
	printf("  transaction_expand_file = %llu\n",
	       (unsigned long long)stats.stats.transaction_expand_file);
	printf("  transaction_read_direct = %llu\n",
	       (unsigned long long)stats.stats.transaction_read_direct);
	printf("    transaction_read_direct_fail = %llu\n",
	       (unsigned long long)stats.stats.transaction_read_direct_fail);
	printf("  transaction_write_direct = %llu\n",
	       (unsigned long long)stats.stats.transaction_write_direct);
	printf("    transaction_write_direct_fail = %llu\n",
	       (unsigned long long)stats.stats.transaction_write_direct_fail);
	printf("expands = %llu\n",
	       (unsigned long long)stats.stats.expands);
	printf("frees = %llu\n",
	       (unsigned long long)stats.stats.frees);
	printf("locks = %llu\n",
	       (unsigned long long)stats.stats.locks);
	printf("  lock_lowlevel = %llu\n",
	       (unsigned long long)stats.stats.lock_lowlevel);
	printf("  lock_nonblock = %llu\n",
	       (unsigned long long)stats.stats.lock_nonblock);
	printf("    lock_nonblock_fail = %llu\n",
	       (unsigned long long)stats.stats.lock_nonblock_fail);

	/* Now clear. */
	tdb_close(*tdb);
	*tdb = tdb_open("/tmp/speed.tdb", flags, O_RDWR, 0, attr);
}

static void tdb_log(struct tdb_context *tdb, enum tdb_log_level level,
		    const char *message, void *data)
{
	fputs(message, stderr);
	putc('\n', stderr);
}

int main(int argc, char *argv[])
{
	unsigned int i, j, num = 1000, stage = 0, stopat = -1;
	int flags = TDB_DEFAULT;
	bool transaction = false, summary = false;
	TDB_DATA key, data;
	struct tdb_context *tdb;
	struct timeval start, stop;
	union tdb_attribute seed, log;
	bool do_stats = false;
	enum TDB_ERROR ecode;

	/* Try to keep benchmarks even. */
	seed.base.attr = TDB_ATTRIBUTE_SEED;
	seed.base.next = NULL;
	seed.seed.seed = 0;

	log.base.attr = TDB_ATTRIBUTE_LOG;
	log.base.next = &seed;
	log.log.fn = tdb_log;

	if (argv[1] && strcmp(argv[1], "--internal") == 0) {
		flags = TDB_INTERNAL;
		argc--;
		argv++;
	}
	if (argv[1] && strcmp(argv[1], "--transaction") == 0) {
		transaction = true;
		argc--;
		argv++;
	}
	if (argv[1] && strcmp(argv[1], "--no-sync") == 0) {
		flags |= TDB_NOSYNC;
		argc--;
		argv++;
	}
	if (argv[1] && strcmp(argv[1], "--summary") == 0) {
		summary = true;
		argc--;
		argv++;
	}
	if (argv[1] && strcmp(argv[1], "--stats") == 0) {
		do_stats = true;
		argc--;
		argv++;
	}

	tdb = tdb_open("/tmp/speed.tdb", flags, O_RDWR|O_CREAT|O_TRUNC,
		       0600, &log);
	if (!tdb)
		err(1, "Opening /tmp/speed.tdb");

	key.dptr = (void *)&i;
	key.dsize = sizeof(i);
	data = key;

	if (argv[1]) {
		num = atoi(argv[1]);
		argv++;
		argc--;
	}

	if (argv[1]) {
		stopat = atoi(argv[1]);
		argv++;
		argc--;
	}

	/* Add 1000 records. */
	printf("Adding %u records: ", num); fflush(stdout);
	if (transaction && (ecode = tdb_transaction_start(tdb)))
		errx(1, "starting transaction: %s", tdb_errorstr(ecode));
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if ((ecode = tdb_store(tdb, key, data, TDB_INSERT)) != 0)
			errx(1, "Inserting key %u in tdb: %s",
			     i, tdb_errorstr(ecode));
	gettimeofday(&stop, NULL);
	if (transaction && (ecode = tdb_transaction_commit(tdb)))
		errx(1, "committing transaction: %s", tdb_errorstr(ecode));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());

	if (tdb_check(tdb, NULL, NULL))
		errx(1, "tdb_check failed!");
	if (summary) {
		char *sumstr = NULL;
		tdb_summary(tdb, TDB_SUMMARY_HISTOGRAMS, &sumstr);
		printf("%s\n", sumstr);
		free(sumstr);
	}
	if (do_stats)
		dump_and_clear_stats(&tdb, flags, &log);

	if (++stage == stopat)
		exit(0);

	/* Finding 1000 records. */
	printf("Finding %u records: ", num); fflush(stdout);
	if (transaction && (ecode = tdb_transaction_start(tdb)))
		errx(1, "starting transaction: %s", tdb_errorstr(ecode));
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++) {
		struct tdb_data dbuf;
		if ((ecode = tdb_fetch(tdb, key, &dbuf)) != TDB_SUCCESS
		    || *(int *)dbuf.dptr != i) {
			errx(1, "Fetching key %u in tdb gave %u",
			     i, ecode ? ecode : *(int *)dbuf.dptr);
		}
	}
	gettimeofday(&stop, NULL);
	if (transaction && (ecode = tdb_transaction_commit(tdb)))
		errx(1, "committing transaction: %s", tdb_errorstr(ecode));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (tdb_check(tdb, NULL, NULL))
		errx(1, "tdb_check failed!");
	if (summary) {
		char *sumstr = NULL;
		tdb_summary(tdb, TDB_SUMMARY_HISTOGRAMS, &sumstr);
		printf("%s\n", sumstr);
		free(sumstr);
	}
	if (do_stats)
		dump_and_clear_stats(&tdb, flags, &log);
	if (++stage == stopat)
		exit(0);

	/* Missing 1000 records. */
	printf("Missing %u records: ", num); fflush(stdout);
	if (transaction && (ecode = tdb_transaction_start(tdb)))
		errx(1, "starting transaction: %s", tdb_errorstr(ecode));
	gettimeofday(&start, NULL);
	for (i = num; i < num*2; i++) {
		struct tdb_data dbuf;
		ecode = tdb_fetch(tdb, key, &dbuf);
		if (ecode != TDB_ERR_NOEXIST)
			errx(1, "Fetching key %u in tdb gave %s",
			     i, tdb_errorstr(ecode));
	}
	gettimeofday(&stop, NULL);
	if (transaction && (ecode = tdb_transaction_commit(tdb)))
		errx(1, "committing transaction: %s", tdb_errorstr(ecode));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (tdb_check(tdb, NULL, NULL))
		errx(1, "tdb_check failed!");
	if (summary) {
		char *sumstr = NULL;
		tdb_summary(tdb, TDB_SUMMARY_HISTOGRAMS, &sumstr);
		printf("%s\n", sumstr);
		free(sumstr);
	}
	if (do_stats)
		dump_and_clear_stats(&tdb, flags, &log);
	if (++stage == stopat)
		exit(0);

	/* Traverse 1000 records. */
	printf("Traversing %u records: ", num); fflush(stdout);
	if (transaction && (ecode = tdb_transaction_start(tdb)))
		errx(1, "starting transaction: %s", tdb_errorstr(ecode));
	i = 0;
	gettimeofday(&start, NULL);
	if (tdb_traverse(tdb, count_record, &i) != num)
		errx(1, "Traverse returned wrong number of records");
	if (i != (num - 1) * (num / 2))
		errx(1, "Traverse tallied to %u", i);
	gettimeofday(&stop, NULL);
	if (transaction && (ecode = tdb_transaction_commit(tdb)))
		errx(1, "committing transaction: %s", tdb_errorstr(ecode));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (tdb_check(tdb, NULL, NULL))
		errx(1, "tdb_check failed!");
	if (summary) {
		char *sumstr = NULL;
		tdb_summary(tdb, TDB_SUMMARY_HISTOGRAMS, &sumstr);
		printf("%s\n", sumstr);
		free(sumstr);
	}
	if (do_stats)
		dump_and_clear_stats(&tdb, flags, &log);
	if (++stage == stopat)
		exit(0);

	/* Delete 1000 records (not in order). */
	printf("Deleting %u records: ", num); fflush(stdout);
	if (transaction && (ecode = tdb_transaction_start(tdb)))
		errx(1, "starting transaction: %s", tdb_errorstr(ecode));
	gettimeofday(&start, NULL);
	for (j = 0; j < num; j++) {
		i = (j + 100003) % num;
		if ((ecode = tdb_delete(tdb, key)) != TDB_SUCCESS)
			errx(1, "Deleting key %u in tdb: %s",
			     i, tdb_errorstr(ecode));
	}
	gettimeofday(&stop, NULL);
	if (transaction && (ecode = tdb_transaction_commit(tdb)))
		errx(1, "committing transaction: %s", tdb_errorstr(ecode));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (tdb_check(tdb, NULL, NULL))
		errx(1, "tdb_check failed!");
	if (summary) {
		char *sumstr = NULL;
		tdb_summary(tdb, TDB_SUMMARY_HISTOGRAMS, &sumstr);
		printf("%s\n", sumstr);
		free(sumstr);
	}
	if (do_stats)
		dump_and_clear_stats(&tdb, flags, &log);
	if (++stage == stopat)
		exit(0);

	/* Re-add 1000 records (not in order). */
	printf("Re-adding %u records: ", num); fflush(stdout);
	if (transaction && (ecode = tdb_transaction_start(tdb)))
		errx(1, "starting transaction: %s", tdb_errorstr(ecode));
	gettimeofday(&start, NULL);
	for (j = 0; j < num; j++) {
		i = (j + 100003) % num;
		if ((ecode = tdb_store(tdb, key, data, TDB_INSERT)) != 0)
			errx(1, "Inserting key %u in tdb: %s",
			     i, tdb_errorstr(ecode));
	}
	gettimeofday(&stop, NULL);
	if (transaction && (ecode = tdb_transaction_commit(tdb)))
		errx(1, "committing transaction: %s", tdb_errorstr(ecode));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (tdb_check(tdb, NULL, NULL))
		errx(1, "tdb_check failed!");
	if (summary) {
		char *sumstr = NULL;
		tdb_summary(tdb, TDB_SUMMARY_HISTOGRAMS, &sumstr);
		printf("%s\n", sumstr);
		free(sumstr);
	}
	if (do_stats)
		dump_and_clear_stats(&tdb, flags, &log);
	if (++stage == stopat)
		exit(0);

	/* Append 1000 records. */
	if (transaction && (ecode = tdb_transaction_start(tdb)))
		errx(1, "starting transaction: %s", tdb_errorstr(ecode));
	printf("Appending %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if ((ecode = tdb_append(tdb, key, data)) != TDB_SUCCESS)
			errx(1, "Appending key %u in tdb: %s",
			     i, tdb_errorstr(ecode));
	gettimeofday(&stop, NULL);
	if (transaction && (ecode = tdb_transaction_commit(tdb)))
		errx(1, "committing transaction: %s", tdb_errorstr(ecode));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (tdb_check(tdb, NULL, NULL))
		errx(1, "tdb_check failed!");
	if (summary) {
		char *sumstr = NULL;
		tdb_summary(tdb, TDB_SUMMARY_HISTOGRAMS, &sumstr);
		printf("%s\n", sumstr);
		free(sumstr);
	}
	if (++stage == stopat)
		exit(0);

	/* Churn 1000 records: not in order! */
	if (transaction && (ecode = tdb_transaction_start(tdb)))
		errx(1, "starting transaction: %s", tdb_errorstr(ecode));
	printf("Churning %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (j = 0; j < num; j++) {
		i = (j + 1000019) % num;
		if ((ecode = tdb_delete(tdb, key)) != TDB_SUCCESS)
			errx(1, "Deleting key %u in tdb: %s",
			     i, tdb_errorstr(ecode));
		i += num;
		if ((ecode = tdb_store(tdb, key, data, TDB_INSERT)) != 0)
			errx(1, "Inserting key %u in tdb: %s",
			     i, tdb_errorstr(ecode));
	}
	gettimeofday(&stop, NULL);
	if (transaction && (ecode = tdb_transaction_commit(tdb)))
		errx(1, "committing transaction: %s", tdb_errorstr(ecode));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());

	if (tdb_check(tdb, NULL, NULL))
		errx(1, "tdb_check failed!");
	if (summary) {
		char *sumstr = NULL;
		tdb_summary(tdb, TDB_SUMMARY_HISTOGRAMS, &sumstr);
		printf("%s\n", sumstr);
		free(sumstr);
	}
	if (do_stats)
		dump_and_clear_stats(&tdb, flags, &log);
	if (++stage == stopat)
		exit(0);

	return 0;
}
