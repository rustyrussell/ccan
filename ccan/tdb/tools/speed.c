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
#include <ccan/tdb/tdb.h>

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

int main(int argc, char *argv[])
{
	unsigned int i, j, num = 1000, stage = 0, stopat = -1;
	int flags = TDB_DEFAULT;
	TDB_DATA key, data;
	struct tdb_context *tdb;
	struct timeval start, stop;
	bool transaction = false;

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

	tdb = tdb_open("/tmp/speed.tdb", 100003, flags, O_RDWR|O_CREAT|O_TRUNC,
		       0600);
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

	return 0;
}
