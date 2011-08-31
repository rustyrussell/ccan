#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <ccan/ilog/ilog.h>
#include "logging.h"

#define MAX_SIZE 13100
#define SIZE_STEP 131

static tdb_off_t tdb_offset(struct tdb_context *tdb, struct tdb_data key)
{
	tdb_off_t off;
	struct tdb_used_record rec;
	struct hash_info h;

	off = find_and_lock(tdb, key, F_RDLCK, &h, &rec, NULL);
	if (TDB_OFF_IS_ERR(off))
		return 0;
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);
	return off;
}

int main(int argc, char *argv[])
{
	unsigned int i, j, moves;
	struct tdb_context *tdb;
	unsigned char *buffer;
	tdb_off_t oldoff = 0, newoff;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data;

	buffer = malloc(MAX_SIZE);
	for (i = 0; i < MAX_SIZE; i++)
		buffer[i] = i;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * ((3 + MAX_SIZE/SIZE_STEP * 5) * 2 + 7)
		   + 1);

	/* Using tdb_store. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-append.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		moves = 0;
		for (j = 0; j < MAX_SIZE; j += SIZE_STEP) {
			data.dptr = buffer;
			data.dsize = j;
			ok1(tdb_store(tdb, key, data, TDB_REPLACE) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
			ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
			ok1(data.dsize == j);
			ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
			free(data.dptr);
			newoff = tdb_offset(tdb, key);
			if (newoff != oldoff)
				moves++;
			oldoff = newoff;
		}
		ok1(!tdb->file || (tdb->file->allrecord_lock.count == 0
				   && tdb->file->num_lockrecs == 0));
		/* We should increase by 50% each time... */
		ok(moves <= ilog64(j / SIZE_STEP)*2, "Moved %u times", moves);
		tdb_close(tdb);
	}

	/* Using tdb_append. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		size_t prev_len = 0;
		tdb = tdb_open("run-append.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		moves = 0;
		for (j = 0; j < MAX_SIZE; j += SIZE_STEP) {
			data.dptr = buffer + prev_len;
			data.dsize = j - prev_len;
			ok1(tdb_append(tdb, key, data) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
			ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
			ok1(data.dsize == j);
			ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
			free(data.dptr);
			prev_len = data.dsize;
			newoff = tdb_offset(tdb, key);
			if (newoff != oldoff)
				moves++;
			oldoff = newoff;
		}
		ok1(!tdb->file || (tdb->file->allrecord_lock.count == 0
				   && tdb->file->num_lockrecs == 0));
		/* We should increase by 50% each time... */
		ok(moves <= ilog64(j / SIZE_STEP)*2, "Moved %u times", moves);
		tdb_close(tdb);
	}

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-append.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Huge initial store. */
		data.dptr = buffer;
		data.dsize = MAX_SIZE;
		ok1(tdb_append(tdb, key, data) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
		ok1(data.dsize == MAX_SIZE);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);
		ok1(!tdb->file || (tdb->file->allrecord_lock.count == 0
				   && tdb->file->num_lockrecs == 0));
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	free(buffer);
	return exit_status();
}
