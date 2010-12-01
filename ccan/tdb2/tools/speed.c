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
#include <ccan/tdb2/tdb2.h>

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

static void dump_and_clear_stats(struct tdb_attribute_stats *stats)
{
	printf("allocs = %llu\n",
	       (unsigned long long)stats->allocs);
	printf("  alloc_subhash = %llu\n",
	       (unsigned long long)stats->alloc_subhash);
	printf("  alloc_chain = %llu\n",
	       (unsigned long long)stats->alloc_chain);
	printf("  alloc_bucket_exact = %llu\n",
	       (unsigned long long)stats->alloc_bucket_exact);
	printf("  alloc_bucket_max = %llu\n",
	       (unsigned long long)stats->alloc_bucket_max);
	printf("  alloc_leftover = %llu\n",
	       (unsigned long long)stats->alloc_leftover);
	printf("  alloc_coalesce_tried = %llu\n",
	       (unsigned long long)stats->alloc_coalesce_tried);
	printf("    alloc_coalesce_lockfail = %llu\n",
	       (unsigned long long)stats->alloc_coalesce_lockfail);
	printf("    alloc_coalesce_race = %llu\n",
	       (unsigned long long)stats->alloc_coalesce_race);
	printf("    alloc_coalesce_succeeded = %llu\n",
	       (unsigned long long)stats->alloc_coalesce_succeeded);
	printf("       alloc_coalesce_num_merged = %llu\n",
	       (unsigned long long)stats->alloc_coalesce_num_merged);
	printf("compares = %llu\n",
	       (unsigned long long)stats->compares);
	printf("  compare_wrong_bucket = %llu\n",
	       (unsigned long long)stats->compare_wrong_bucket);
	printf("  compare_wrong_offsetbits = %llu\n",
	       (unsigned long long)stats->compare_wrong_offsetbits);
	printf("  compare_wrong_keylen = %llu\n",
	       (unsigned long long)stats->compare_wrong_keylen);
	printf("  compare_wrong_rechash = %llu\n",
	       (unsigned long long)stats->compare_wrong_rechash);
	printf("  compare_wrong_keycmp = %llu\n",
	       (unsigned long long)stats->compare_wrong_keycmp);
	printf("expands = %llu\n",
	       (unsigned long long)stats->expands);
	printf("frees = %llu\n",
	       (unsigned long long)stats->frees);
	printf("locks = %llu\n",
	       (unsigned long long)stats->locks);
	printf("   lock_lowlevel = %llu\n",
	       (unsigned long long)stats->lock_lowlevel);
	printf("   lock_nonblock = %llu\n",
	       (unsigned long long)stats->lock_nonblock);

	/* Now clear. */
	memset(&stats->allocs, 0, (char *)(stats+1) - (char *)&stats->allocs);
}

int main(int argc, char *argv[])
{
	unsigned int i, j, num = 1000, stage = 0, stopat = -1;
	int flags = TDB_DEFAULT;
	bool transaction = false;
	TDB_DATA key, data;
	struct tdb_context *tdb;
	struct timeval start, stop;
	union tdb_attribute seed, stats;

	/* Try to keep benchmarks even. */
	seed.base.attr = TDB_ATTRIBUTE_SEED;
	seed.base.next = NULL;
	seed.seed.seed = 0;

	memset(&stats, 0, sizeof(stats));
	stats.base.attr = TDB_ATTRIBUTE_STATS;
	stats.base.next = NULL;
	stats.stats.size = sizeof(stats);

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
	if (argv[1] && strcmp(argv[1], "--stats") == 0) {
		seed.base.next = &stats;
		argc--;
		argv++;
	}

	tdb = tdb_open("/tmp/speed.tdb", flags, O_RDWR|O_CREAT|O_TRUNC,
		       0600, &seed);
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

	if (transaction && tdb_transaction_start(tdb))
		errx(1, "starting transaction: %s", tdb_errorstr(tdb));

	/* Add 1000 records. */
	printf("Adding %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if (tdb_store(tdb, key, data, TDB_INSERT) != 0)
			errx(1, "Inserting key %u in tdb: %s",
			     i, tdb_errorstr(tdb));
	gettimeofday(&stop, NULL);
	if (transaction && tdb_transaction_commit(tdb))
		errx(1, "committing transaction: %s", tdb_errorstr(tdb));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());

	if (seed.base.next)
		dump_and_clear_stats(&stats.stats);
	if (++stage == stopat)
		exit(0);

	if (transaction && tdb_transaction_start(tdb))
		errx(1, "starting transaction: %s", tdb_errorstr(tdb));

	/* Finding 1000 records. */
	printf("Finding %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++) {
		int *dptr;
		dptr = (int *)tdb_fetch(tdb, key).dptr;
		if (!dptr || *dptr != i)
			errx(1, "Fetching key %u in tdb gave %u",
			     i, dptr ? *dptr : -1);
	}
	gettimeofday(&stop, NULL);
	if (transaction && tdb_transaction_commit(tdb))
		errx(1, "committing transaction: %s", tdb_errorstr(tdb));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (seed.base.next)
		dump_and_clear_stats(&stats.stats);
	if (++stage == stopat)
		exit(0);

	if (transaction && tdb_transaction_start(tdb))
		errx(1, "starting transaction: %s", tdb_errorstr(tdb));

	/* Missing 1000 records. */
	printf("Missing %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = num; i < num*2; i++) {
		int *dptr;
		dptr = (int *)tdb_fetch(tdb, key).dptr;
		if (dptr)
			errx(1, "Fetching key %u in tdb gave %u", i, *dptr);
	}
	gettimeofday(&stop, NULL);
	if (transaction && tdb_transaction_commit(tdb))
		errx(1, "committing transaction: %s", tdb_errorstr(tdb));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (seed.base.next)
		dump_and_clear_stats(&stats.stats);
	if (++stage == stopat)
		exit(0);

	if (transaction && tdb_transaction_start(tdb))
		errx(1, "starting transaction: %s", tdb_errorstr(tdb));

	/* Traverse 1000 records. */
	printf("Traversing %u records: ", num); fflush(stdout);
	i = 0;
	gettimeofday(&start, NULL);
	if (tdb_traverse(tdb, count_record, &i) != num)
		errx(1, "Traverse returned wrong number of records");
	if (i != (num - 1) * (num / 2))
		errx(1, "Traverse tallied to %u", i);
	gettimeofday(&stop, NULL);
	if (transaction && tdb_transaction_commit(tdb))
		errx(1, "committing transaction: %s", tdb_errorstr(tdb));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (seed.base.next)
		dump_and_clear_stats(&stats.stats);
	if (++stage == stopat)
		exit(0);

	if (transaction && tdb_transaction_start(tdb))
		errx(1, "starting transaction: %s", tdb_errorstr(tdb));

	/* Delete 1000 records (not in order). */
	printf("Deleting %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (j = 0; j < num; j++) {
		i = (j + 100003) % num;
		if (tdb_delete(tdb, key) != 0)
			errx(1, "Deleting key %u in tdb: %s",
			     i, tdb_errorstr(tdb));
	}
	gettimeofday(&stop, NULL);
	if (transaction && tdb_transaction_commit(tdb))
		errx(1, "committing transaction: %s", tdb_errorstr(tdb));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (seed.base.next)
		dump_and_clear_stats(&stats.stats);
	if (++stage == stopat)
		exit(0);

	if (transaction && tdb_transaction_start(tdb))
		errx(1, "starting transaction: %s", tdb_errorstr(tdb));

	/* Re-add 1000 records (not in order). */
	printf("Re-adding %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (j = 0; j < num; j++) {
		i = (j + 100003) % num;
		if (tdb_store(tdb, key, data, TDB_INSERT) != 0)
			errx(1, "Inserting key %u in tdb: %s",
			     i, tdb_errorstr(tdb));
	}
	gettimeofday(&stop, NULL);
	if (transaction && tdb_transaction_commit(tdb))
		errx(1, "committing transaction: %s", tdb_errorstr(tdb));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (seed.base.next)
		dump_and_clear_stats(&stats.stats);
	if (++stage == stopat)
		exit(0);

	if (transaction && tdb_transaction_start(tdb))
		errx(1, "starting transaction: %s", tdb_errorstr(tdb));

	/* Append 1000 records. */
	printf("Appending %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if (tdb_append(tdb, key, data) != 0)
			errx(1, "Appending key %u in tdb: %s",
			     i, tdb_errorstr(tdb));
	gettimeofday(&stop, NULL);
	if (transaction && tdb_transaction_commit(tdb))
		errx(1, "committing transaction: %s", tdb_errorstr(tdb));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());
	if (++stage == stopat)
		exit(0);

	if (transaction && tdb_transaction_start(tdb))
		errx(1, "starting transaction: %s", tdb_errorstr(tdb));

	/* Churn 1000 records: not in order! */
	printf("Churning %u records: ", num); fflush(stdout);
	gettimeofday(&start, NULL);
	for (j = 0; j < num; j++) {
		i = (j + 1000019) % num;
		if (tdb_delete(tdb, key) != 0)
			errx(1, "Deleting key %u in tdb: %s",
			     i, tdb_errorstr(tdb));
		i += num;
		if (tdb_store(tdb, key, data, TDB_INSERT) != 0)
			errx(1, "Inserting key %u in tdb: %s",
			     i, tdb_errorstr(tdb));
	}
	gettimeofday(&stop, NULL);
	if (transaction && tdb_transaction_commit(tdb))
		errx(1, "committing transaction: %s", tdb_errorstr(tdb));
	printf(" %zu ns (%zu bytes)\n",
	       normalize(&start, &stop, num), file_size());

	if (seed.base.next)
		dump_and_clear_stats(&stats.stats);
	if (++stage == stopat)
		exit(0);

	return 0;
}
