#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;

	plan_tests(1 + TDB_ERR_RDONLY*-1 + 2);
	tdb = tdb_open("run-tdb_errorstr.tdb", TDB_DEFAULT,
		       O_RDWR|O_CREAT|O_TRUNC, 0600, NULL);
	ok1(tdb);
	if (tdb) {
		enum TDB_ERROR err;
		for (err = TDB_SUCCESS; err >= TDB_ERR_RDONLY; err--) {
			tdb->ecode = err;
			switch (err) {
			case TDB_SUCCESS:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "Success"));
				break;
			case TDB_ERR_IO:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "IO Error"));
				break;
			case TDB_ERR_LOCK:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "Locking error"));
				break;
			case TDB_ERR_OOM:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "Out of memory"));
				break;
			case TDB_ERR_EXISTS:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "Record exists"));
				break;
			case TDB_ERR_EINVAL:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "Invalid parameter"));
				break;
			case TDB_ERR_NOEXIST:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "Record does not exist"));
				break;
			case TDB_ERR_RDONLY:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "write not permitted"));
				break;
			case TDB_ERR_CORRUPT:
				ok1(!strcmp(tdb_errorstr(tdb),
					    "Corrupt database"));
			}
		}
		tdb->ecode = err;
		ok1(!strcmp(tdb_errorstr(tdb), "Invalid error code"));
	}
	return exit_status();
}
