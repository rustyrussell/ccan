#include "tdb2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static void logfn(struct tdb_context *tdb,
		  enum tdb_log_level level,
		  const char *message,
		  void *data)
{
	fprintf(stderr, "tdb:%s:%s\n", tdb_name(tdb), message);
}

int main(int argc, char *argv[])
{
	unsigned int i, j, users, groups;
	TDB_DATA idxkey, idxdata;
	TDB_DATA k, d, gk;
	char cmd[100];
	struct tdb_context *tdb;
	enum TDB_ERROR ecode;
	union tdb_attribute log;

	if (argc != 3) {
		printf("Usage: growtdb-bench <users> <groups>\n");
		exit(1);
	}
	users = atoi(argv[1]);
	groups = atoi(argv[2]);

	sprintf(cmd, "cat /proc/%i/statm", getpid());

	log.base.attr = TDB_ATTRIBUTE_LOG;
	log.base.next = NULL;
	log.log.fn = logfn;
	
	tdb = tdb_open("/tmp/growtdb.tdb", TDB_DEFAULT,
		       O_RDWR|O_CREAT|O_TRUNC, 0600, &log);

	idxkey.dptr = (unsigned char *)"User index";
	idxkey.dsize = strlen("User index");
	idxdata.dsize = 51;
	idxdata.dptr = calloc(idxdata.dsize, 1);

	/* Create users. */
	k.dsize = 48;
	k.dptr = calloc(k.dsize, 1);
	d.dsize = 64;
	d.dptr = calloc(d.dsize, 1);

	tdb_transaction_start(tdb);
	for (i = 0; i < users; i++) {
		memcpy(k.dptr, &i, sizeof(i));
		ecode = tdb_store(tdb, k, d, TDB_INSERT);
		if (ecode != TDB_SUCCESS)
			errx(1, "tdb insert failed: %s", tdb_errorstr(ecode));

		/* This simulates a growing index record. */
		ecode = tdb_append(tdb, idxkey, idxdata);
		if (ecode != TDB_SUCCESS)
			errx(1, "tdb append failed: %s", tdb_errorstr(ecode));
	}
	if ((ecode = tdb_transaction_commit(tdb)) != 0)
		errx(1, "tdb commit1 failed: %s", tdb_errorstr(ecode));

	if ((ecode = tdb_check(tdb, NULL, NULL)) != 0)
		errx(1, "tdb_check failed after initial insert!");

	system(cmd);

	/* Now put them all in groups: add 32 bytes to each record for
	 * a group. */
	gk.dsize = 48;
	gk.dptr = calloc(k.dsize, 1);
	gk.dptr[gk.dsize-1] = 1;

	d.dsize = 32;
	for (i = 0; i < groups; i++) {
		tdb_transaction_start(tdb);
		/* Create the "group". */
		memcpy(gk.dptr, &i, sizeof(i));
		ecode = tdb_store(tdb, gk, d, TDB_INSERT);
		if (ecode != TDB_SUCCESS)
			errx(1, "tdb insert failed: %s", tdb_errorstr(ecode));

		/* Now populate it. */
		for (j = 0; j < users; j++) {
			/* Append to the user. */
			memcpy(k.dptr, &j, sizeof(j));
			if ((ecode = tdb_append(tdb, k, d)) != 0)
				errx(1, "tdb append failed: %s",
				     tdb_errorstr(ecode));
			
			/* Append to the group. */
			if ((ecode = tdb_append(tdb, gk, d)) != 0)
				errx(1, "tdb append failed: %s",
				     tdb_errorstr(ecode));
		}
		if ((ecode = tdb_transaction_commit(tdb)) != 0)
			errx(1, "tdb commit2 failed: %s", tdb_errorstr(ecode));
		if ((ecode = tdb_check(tdb, NULL, NULL)) != 0)
			errx(1, "tdb_check failed after iteration %i!", i);
		system(cmd);
	}

	return 0;
}
