#include "tdb2-source.h"
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	enum TDB_ERROR err;
	plan_tests(TDB_ERR_RDONLY*-1 + 2);

	for (err = TDB_SUCCESS; err >= TDB_ERR_RDONLY; err--) {
		switch (err) {
		case TDB_SUCCESS:
			ok1(!strcmp(tdb_errorstr(err),
				    "Success"));
			break;
		case TDB_ERR_IO:
			ok1(!strcmp(tdb_errorstr(err),
				    "IO Error"));
			break;
		case TDB_ERR_LOCK:
			ok1(!strcmp(tdb_errorstr(err),
				    "Locking error"));
			break;
		case TDB_ERR_OOM:
			ok1(!strcmp(tdb_errorstr(err),
				    "Out of memory"));
			break;
		case TDB_ERR_EXISTS:
			ok1(!strcmp(tdb_errorstr(err),
				    "Record exists"));
			break;
		case TDB_ERR_EINVAL:
			ok1(!strcmp(tdb_errorstr(err),
				    "Invalid parameter"));
			break;
		case TDB_ERR_NOEXIST:
			ok1(!strcmp(tdb_errorstr(err),
				    "Record does not exist"));
			break;
		case TDB_ERR_RDONLY:
			ok1(!strcmp(tdb_errorstr(err),
				    "write not permitted"));
			break;
		case TDB_ERR_CORRUPT:
			ok1(!strcmp(tdb_errorstr(err),
				    "Corrupt database"));
			break;
		}
	}
	ok1(!strcmp(tdb_errorstr(err), "Invalid error code"));

	return exit_status();
}
