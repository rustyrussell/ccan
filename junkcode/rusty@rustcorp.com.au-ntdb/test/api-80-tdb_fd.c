#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 3);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("api-80-ntdb_fd.ntdb", flags[i]|MAYBE_NOSYNC,
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(ntdb))
			continue;

		if (flags[i] & NTDB_INTERNAL)
			ok1(ntdb_fd(ntdb) == -1);
		else
			ok1(ntdb_fd(ntdb) > 2);
		ntdb_close(ntdb);
		ok1(tap_log_messages == 0);
	}
	return exit_status();
}
