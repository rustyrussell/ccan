#include "ntdb-source.h"
#include "tap-interface.h"
#include "helprun-external-agent.h"

int main(int argc, char *argv[])
{
	enum NTDB_ERROR e;
	plan_tests(NTDB_ERR_RDONLY*-1 + 2);

	for (e = NTDB_SUCCESS; e >= NTDB_ERR_RDONLY; e--) {
		switch (e) {
		case NTDB_SUCCESS:
			ok1(!strcmp(ntdb_errorstr(e),
				    "Success"));
			break;
		case NTDB_ERR_IO:
			ok1(!strcmp(ntdb_errorstr(e),
				    "IO Error"));
			break;
		case NTDB_ERR_LOCK:
			ok1(!strcmp(ntdb_errorstr(e),
				    "Locking error"));
			break;
		case NTDB_ERR_OOM:
			ok1(!strcmp(ntdb_errorstr(e),
				    "Out of memory"));
			break;
		case NTDB_ERR_EXISTS:
			ok1(!strcmp(ntdb_errorstr(e),
				    "Record exists"));
			break;
		case NTDB_ERR_EINVAL:
			ok1(!strcmp(ntdb_errorstr(e),
				    "Invalid parameter"));
			break;
		case NTDB_ERR_NOEXIST:
			ok1(!strcmp(ntdb_errorstr(e),
				    "Record does not exist"));
			break;
		case NTDB_ERR_RDONLY:
			ok1(!strcmp(ntdb_errorstr(e),
				    "write not permitted"));
			break;
		case NTDB_ERR_CORRUPT:
			ok1(!strcmp(ntdb_errorstr(e),
				    "Corrupt database"));
			break;
		}
	}
	ok1(!strcmp(ntdb_errorstr(e), "Invalid error code"));

	return exit_status();
}
