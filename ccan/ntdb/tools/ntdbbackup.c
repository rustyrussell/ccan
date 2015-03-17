/*
   Unix SMB/CIFS implementation.
   low level ntdb backup and restore utility
   Copyright (C) Andrew Tridgell              2002

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*

  This program is meant for backup/restore of ntdb databases. Typical usage would be:
     tdbbackup *.ntdb
  when Samba shuts down cleanly, which will make a backup of all the local databases
  to *.bak files. Then on Samba startup you would use:
     tdbbackup -v *.ntdb
  and this will check the databases for corruption and if corruption is detected then
  the backup will be restored.

  You may also like to do a backup on a regular basis while Samba is
  running, perhaps using cron.

  The reason this program is needed is to cope with power failures
  while Samba is running. A power failure could lead to database
  corruption and Samba will then not start correctly.

  Note that many of the databases in Samba are transient and thus
  don't need to be backed up, so you can optimise the above a little
  by only running the backup on the critical databases.

 */

#include "config.h"
#include "ntdb.h"
#include "private.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

static int failed;

static void ntdb_log(struct ntdb_context *ntdb,
		    enum ntdb_log_level level,
		    enum NTDB_ERROR ecode,
		    const char *message,
		    void *data)
{
	fprintf(stderr, "%s:%s\n", ntdb_errorstr(ecode), message);
}

static char *add_suffix(const char *name, const char *suffix)
{
	char *ret;
	int len = strlen(name) + strlen(suffix) + 1;
	ret = (char *)malloc(len);
	if (!ret) {
		fprintf(stderr,"Out of memory!\n");
		exit(1);
	}
	snprintf(ret, len, "%s%s", name, suffix);
	return ret;
}

static int copy_fn(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf, void *state)
{
	struct ntdb_context *ntdb_new = (struct ntdb_context *)state;
	enum NTDB_ERROR err;

	err = ntdb_store(ntdb_new, key, dbuf, NTDB_INSERT);
	if (err) {
		fprintf(stderr,"Failed to insert into %s: %s\n",
			ntdb_name(ntdb_new), ntdb_errorstr(err));
		failed = 1;
		return 1;
	}
	return 0;
}


static int test_fn(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf, void *state)
{
	return 0;
}

/*
  carefully backup a ntdb, validating the contents and
  only doing the backup if its OK
  this function is also used for restore
*/
static int backup_ntdb(const char *old_name, const char *new_name)
{
	struct ntdb_context *ntdb;
	struct ntdb_context *ntdb_new;
	char *tmp_name;
	struct stat st;
	int count1, count2;
	enum NTDB_ERROR err;
	union ntdb_attribute log_attr;

	tmp_name = add_suffix(new_name, ".tmp");

	/* stat the old ntdb to find its permissions */
	if (stat(old_name, &st) != 0) {
		perror(old_name);
		free(tmp_name);
		return 1;
	}

	log_attr.base.attr = NTDB_ATTRIBUTE_LOG;
	log_attr.base.next = NULL;
	log_attr.log.fn = ntdb_log;

	/* open the old ntdb */
	ntdb = ntdb_open(old_name, NTDB_DEFAULT, O_RDWR, 0, &log_attr);
	if (!ntdb) {
		printf("Failed to open %s\n", old_name);
		free(tmp_name);
		return 1;
	}

	unlink(tmp_name);
	ntdb_new = ntdb_open(tmp_name, NTDB_DEFAULT,
			   O_RDWR|O_CREAT|O_EXCL, st.st_mode & 0777,
			   &log_attr);
	if (!ntdb_new) {
		perror(tmp_name);
		free(tmp_name);
		return 1;
	}

	err = ntdb_transaction_start(ntdb);
	if (err) {
		fprintf(stderr, "Failed to start transaction on old ntdb: %s\n",
			ntdb_errorstr(err));
		ntdb_close(ntdb);
		ntdb_close(ntdb_new);
		unlink(tmp_name);
		free(tmp_name);
		return 1;
	}

	/* lock the backup ntdb so that nobody else can change it */
	err = ntdb_lockall(ntdb_new);
	if (err) {
		fprintf(stderr, "Failed to lock backup ntdb: %s\n",
			ntdb_errorstr(err));
		ntdb_close(ntdb);
		ntdb_close(ntdb_new);
		unlink(tmp_name);
		free(tmp_name);
		return 1;
	}

	failed = 0;

	/* traverse and copy */
	count1 = ntdb_traverse(ntdb, copy_fn, (void *)ntdb_new);
	if (count1 < 0 || failed) {
		fprintf(stderr,"failed to copy %s\n", old_name);
		ntdb_close(ntdb);
		ntdb_close(ntdb_new);
		unlink(tmp_name);
		free(tmp_name);
		return 1;
	}

	/* close the old ntdb */
	ntdb_close(ntdb);

	/* copy done, unlock the backup ntdb */
	ntdb_unlockall(ntdb_new);

#ifdef HAVE_FDATASYNC
	if (fdatasync(ntdb_fd(ntdb_new)) != 0) {
#else
	if (fsync(ntdb_fd(ntdb_new)) != 0) {
#endif
		/* not fatal */
		fprintf(stderr, "failed to fsync backup file\n");
	}

	/* close the new ntdb and re-open read-only */
	ntdb_close(ntdb_new);

	/* we don't need the hash attr any more */
	log_attr.base.next = NULL;

	ntdb_new = ntdb_open(tmp_name, NTDB_DEFAULT, O_RDONLY, 0, &log_attr);
	if (!ntdb_new) {
		fprintf(stderr,"failed to reopen %s\n", tmp_name);
		unlink(tmp_name);
		perror(tmp_name);
		free(tmp_name);
		return 1;
	}

	/* traverse the new ntdb to confirm */
	count2 = ntdb_traverse(ntdb_new, test_fn, NULL);
	if (count2 != count1) {
		fprintf(stderr,"failed to copy %s\n", old_name);
		ntdb_close(ntdb_new);
		unlink(tmp_name);
		free(tmp_name);
		return 1;
	}

	/* close the new ntdb and rename it to .bak */
	ntdb_close(ntdb_new);
	if (rename(tmp_name, new_name) != 0) {
		perror(new_name);
		free(tmp_name);
		return 1;
	}

	free(tmp_name);

	return 0;
}

/*
  verify a ntdb and if it is corrupt then restore from *.bak
*/
static int verify_ntdb(const char *fname, const char *bak_name)
{
	struct ntdb_context *ntdb;
	int count = -1;
	union ntdb_attribute log_attr;

	log_attr.base.attr = NTDB_ATTRIBUTE_LOG;
	log_attr.base.next = NULL;
	log_attr.log.fn = ntdb_log;

	/* open the ntdb */
	ntdb = ntdb_open(fname, NTDB_DEFAULT, O_RDONLY, 0, &log_attr);

	/* traverse the ntdb, then close it */
	if (ntdb) {
		count = ntdb_traverse(ntdb, test_fn, NULL);
		ntdb_close(ntdb);
	}

	/* count is < 0 means an error */
	if (count < 0) {
		printf("restoring %s\n", fname);
		return backup_ntdb(bak_name, fname);
	}

	printf("%s : %d records\n", fname, count);

	return 0;
}

/*
  see if one file is newer than another
*/
static int file_newer(const char *fname1, const char *fname2)
{
	struct stat st1, st2;
	if (stat(fname1, &st1) != 0) {
		return 0;
	}
	if (stat(fname2, &st2) != 0) {
		return 1;
	}
	return (st1.st_mtime > st2.st_mtime);
}

static void usage(void)
{
	printf("Usage: ntdbbackup [options] <fname...>\n\n");
	printf("   -h            this help message\n");
	printf("   -v            verify mode (restore if corrupt)\n");
	printf("   -s suffix     set the backup suffix\n");
	printf("   -v            verify mode (restore if corrupt)\n");
}


 int main(int argc, char *argv[])
{
	int i;
	int ret = 0;
	int c;
	int verify = 0;
	const char *suffix = ".bak";

	while ((c = getopt(argc, argv, "vhs:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit(0);
		case 'v':
			verify = 1;
			break;
		case 's':
			suffix = optarg;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
		exit(1);
	}

	for (i=0; i<argc; i++) {
		const char *fname = argv[i];
		char *bak_name;

		bak_name = add_suffix(fname, suffix);

		if (verify) {
			if (verify_ntdb(fname, bak_name) != 0) {
				ret = 1;
			}
		} else {
			if (file_newer(fname, bak_name) &&
			    backup_ntdb(fname, bak_name) != 0) {
				ret = 1;
			}
		}

		free(bak_name);
	}

	return ret;
}
