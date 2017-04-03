#include "ntdb.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ccan/err/err.h>

static void logfn(struct ntdb_context *ntdb,
		  enum ntdb_log_level level,
		  enum NTDB_ERROR ecode,
		  const char *message,
		  void *data)
{
	fprintf(stderr, "ntdb:%s:%s:%s\n",
		ntdb_name(ntdb), ntdb_errorstr(ecode), message);
}

int main(int argc, char *argv[])
{
	unsigned int i, j, users, groups;
	NTDB_DATA idxkey, idxdata;
	NTDB_DATA k, d, gk;
	char cmd[100];
	struct ntdb_context *ntdb;
	enum NTDB_ERROR ecode;
	union ntdb_attribute log;

	if (argc != 3) {
		printf("Usage: growtdb-bench <users> <groups>\n");
		exit(1);
	}
	users = atoi(argv[1]);
	groups = atoi(argv[2]);

	sprintf(cmd, "cat /proc/%i/statm", getpid());

	log.base.attr = NTDB_ATTRIBUTE_LOG;
	log.base.next = NULL;
	log.log.fn = logfn;

	ntdb = ntdb_open("/tmp/growtdb.ntdb", NTDB_DEFAULT,
		       O_RDWR|O_CREAT|O_TRUNC, 0600, &log);

	idxkey.dptr = (unsigned char *)"User index";
	idxkey.dsize = strlen("User index");
	idxdata.dsize = 51;
	idxdata.dptr = calloc(idxdata.dsize, 1);
	if (idxdata.dptr == NULL) {
		fprintf(stderr, "Unable to allocate memory for idxdata.dptr\n");
		return -1;
	}

	/* Create users. */
	k.dsize = 48;
	k.dptr = calloc(k.dsize, 1);
	if (k.dptr == NULL) {
		fprintf(stderr, "Unable to allocate memory for k.dptr\n");
		return -1;
	}
	d.dsize = 64;
	d.dptr = calloc(d.dsize, 1);
	if (d.dptr == NULL) {
		fprintf(stderr, "Unable to allocate memory for d.dptr\n");
		return -1;
	}

	ntdb_transaction_start(ntdb);
	for (i = 0; i < users; i++) {
		memcpy(k.dptr, &i, sizeof(i));
		ecode = ntdb_store(ntdb, k, d, NTDB_INSERT);
		if (ecode != NTDB_SUCCESS)
			errx(1, "ntdb insert failed: %s", ntdb_errorstr(ecode));

		/* This simulates a growing index record. */
		ecode = ntdb_append(ntdb, idxkey, idxdata);
		if (ecode != NTDB_SUCCESS)
			errx(1, "ntdb append failed: %s", ntdb_errorstr(ecode));
	}
	if ((ecode = ntdb_transaction_commit(ntdb)) != 0)
		errx(1, "ntdb commit1 failed: %s", ntdb_errorstr(ecode));

	if ((ecode = ntdb_check(ntdb, NULL, NULL)) != 0)
		errx(1, "ntdb_check failed after initial insert!");

	system(cmd);

	/* Now put them all in groups: add 32 bytes to each record for
	 * a group. */
	gk.dsize = 48;
	gk.dptr = calloc(k.dsize, 1);
	if (gk.dptr == NULL) {
		fprintf(stderr, "Unable to allocate memory for gk.dptr\n");
		return -1;
	}
	gk.dptr[gk.dsize-1] = 1;

	d.dsize = 32;
	for (i = 0; i < groups; i++) {
		ntdb_transaction_start(ntdb);
		/* Create the "group". */
		memcpy(gk.dptr, &i, sizeof(i));
		ecode = ntdb_store(ntdb, gk, d, NTDB_INSERT);
		if (ecode != NTDB_SUCCESS)
			errx(1, "ntdb insert failed: %s", ntdb_errorstr(ecode));

		/* Now populate it. */
		for (j = 0; j < users; j++) {
			/* Append to the user. */
			memcpy(k.dptr, &j, sizeof(j));
			if ((ecode = ntdb_append(ntdb, k, d)) != 0)
				errx(1, "ntdb append failed: %s",
				     ntdb_errorstr(ecode));

			/* Append to the group. */
			if ((ecode = ntdb_append(ntdb, gk, d)) != 0)
				errx(1, "ntdb append failed: %s",
				     ntdb_errorstr(ecode));
		}
		if ((ecode = ntdb_transaction_commit(ntdb)) != 0)
			errx(1, "ntdb commit2 failed: %s", ntdb_errorstr(ecode));
		if ((ecode = ntdb_check(ntdb, NULL, NULL)) != 0)
			errx(1, "ntdb_check failed after iteration %i!", i);
		system(cmd);
	}

	return 0;
}
