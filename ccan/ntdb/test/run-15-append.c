#include "ntdb-source.h"
#include "tap-interface.h"
#include <ccan/ilog/ilog.h>
#include "logging.h"

#define MAX_SIZE 13100
#define SIZE_STEP 131

static ntdb_off_t ntdb_offset(struct ntdb_context *ntdb, NTDB_DATA key)
{
	ntdb_off_t off;
	struct ntdb_used_record urec;
	struct hash_info h;

	off = find_and_lock(ntdb, key, F_RDLCK, &h, &urec, NULL);
	if (NTDB_OFF_IS_ERR(off))
		return 0;
	ntdb_unlock_hash(ntdb, h.h, F_RDLCK);
	return off;
}

int main(int argc, char *argv[])
{
	unsigned int i, j, moves;
	struct ntdb_context *ntdb;
	unsigned char *buffer;
	ntdb_off_t oldoff = 0, newoff;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data;

	buffer = malloc(MAX_SIZE);
	for (i = 0; i < MAX_SIZE; i++)
		buffer[i] = i;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * ((3 + MAX_SIZE/SIZE_STEP * 5) * 2 + 7)
		   + 1);

	/* Using ntdb_store. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-append.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		moves = 0;
		for (j = 0; j < MAX_SIZE; j += SIZE_STEP) {
			data.dptr = buffer;
			data.dsize = j;
			ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == 0);
			ok1(ntdb_check(ntdb, NULL, NULL) == 0);
			ok1(ntdb_fetch(ntdb, key, &data) == NTDB_SUCCESS);
			ok1(data.dsize == j);
			ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
			free(data.dptr);
			newoff = ntdb_offset(ntdb, key);
			if (newoff != oldoff)
				moves++;
			oldoff = newoff;
		}
		ok1(!ntdb->file || (ntdb->file->allrecord_lock.count == 0
				   && ntdb->file->num_lockrecs == 0));
		/* We should increase by 50% each time... */
		ok(moves <= ilog64(j / SIZE_STEP)*2,
		   "Moved %u times", moves);
		ntdb_close(ntdb);
	}

	/* Using ntdb_append. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		size_t prev_len = 0;
		ntdb = ntdb_open("run-append.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		moves = 0;
		for (j = 0; j < MAX_SIZE; j += SIZE_STEP) {
			data.dptr = buffer + prev_len;
			data.dsize = j - prev_len;
			ok1(ntdb_append(ntdb, key, data) == 0);
			ok1(ntdb_check(ntdb, NULL, NULL) == 0);
			ok1(ntdb_fetch(ntdb, key, &data) == NTDB_SUCCESS);
			ok1(data.dsize == j);
			ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
			free(data.dptr);
			prev_len = data.dsize;
			newoff = ntdb_offset(ntdb, key);
			if (newoff != oldoff)
				moves++;
			oldoff = newoff;
		}
		ok1(!ntdb->file || (ntdb->file->allrecord_lock.count == 0
				   && ntdb->file->num_lockrecs == 0));
		/* We should increase by 50% each time... */
		ok(moves <= ilog64(j / SIZE_STEP)*2,
		   "Moved %u times", moves);
		ntdb_close(ntdb);
	}

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-append.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		/* Huge initial store. */
		data.dptr = buffer;
		data.dsize = MAX_SIZE;
		ok1(ntdb_append(ntdb, key, data) == 0);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(ntdb_fetch(ntdb, key, &data) == NTDB_SUCCESS);
		ok1(data.dsize == MAX_SIZE);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);
		ok1(!ntdb->file || (ntdb->file->allrecord_lock.count == 0
				   && ntdb->file->num_lockrecs == 0));
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	free(buffer);
	return exit_status();
}
