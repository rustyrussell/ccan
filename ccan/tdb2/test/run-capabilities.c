#include <ccan/failtest/failtest_override.h>
#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include "logging.h"
#include "layout.h"
#include "failtest_helper.h"
#include <stdarg.h>
#include <err.h>

static size_t len_of(bool breaks_check, bool breaks_write, bool breaks_open)
{
	size_t len = 0;
	if (breaks_check)
		len += 8;
	if (breaks_write)
		len += 16;
	if (breaks_open)
		len += 32;
	return len;
}

/* Creates a TDB with various capabilities. */
static void create_tdb(const char *name,
		       unsigned int cap,
		       bool breaks_check,
		       bool breaks_write,
		       bool breaks_open, ...)
{
	TDB_DATA key, data;
	va_list ap;
	struct tdb_layout *layout;
	struct tdb_context *tdb;
	int fd;

	key = tdb_mkdata("Hello", 5);
	data = tdb_mkdata("world", 5);

	/* Create a TDB with some data, and some capabilities */
	layout = new_tdb_layout();
	tdb_layout_add_freetable(layout);
	tdb_layout_add_used(layout, key, data, 6);
	tdb_layout_add_free(layout, 80, 0);
	tdb_layout_add_capability(layout, cap,
				  breaks_write, breaks_check, breaks_open,
				  len_of(breaks_check, breaks_write, breaks_open));

	va_start(ap, breaks_open);
	while ((cap = va_arg(ap, int)) != 0) {
		breaks_check = va_arg(ap, int);
		breaks_write = va_arg(ap, int);
		breaks_open = va_arg(ap, int);

		key.dsize--;
		tdb_layout_add_used(layout, key, data, 11 - key.dsize);
		tdb_layout_add_free(layout, 80, 0);
		tdb_layout_add_capability(layout, cap,
					  breaks_write, breaks_check,
					  breaks_open,
					  len_of(breaks_check, breaks_write,
						 breaks_open));
	}
	va_end(ap);

	/* We open-code this, because we need to use the failtest write. */
	tdb = tdb_layout_get(layout, failtest_free, &tap_log_attr);

	fd = open(name, O_RDWR|O_TRUNC|O_CREAT, 0600);
	if (fd < 0)
		err(1, "opening %s for writing", name);
	if (write(fd, tdb->file->map_ptr, tdb->file->map_size)
	    != tdb->file->map_size)
		err(1, "writing %s", name);
	close(fd);
	tdb_close(tdb);
	tdb_layout_free(layout);
}

/* Note all the "goto out" early exits: they're to shorten failtest time. */
int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	char *summary;

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;
	plan_tests(60);

	failtest_suppress = true;
	/* Capability says you can ignore it? */
	create_tdb("run-capabilities.tdb", 1, false, false, false, 0);

	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(tdb))
		goto out;
	ok1(tap_log_messages == 0);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	ok1(tap_log_messages == 0);
	tdb_close(tdb);

	/* Two capabilitues say you can ignore them? */
	create_tdb("run-capabilities.tdb",
		   1, false, false, false,
		   2, false, false, false, 0);

	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(tdb))
		goto out;
	ok1(tap_log_messages == 0);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	ok1(tap_log_messages == 0);
	ok1(tdb_summary(tdb, 0, &summary) == TDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	free(summary);
	tdb_close(tdb);

	/* Capability says you can't check. */
	create_tdb("run-capabilities.tdb",
		   1, false, false, false,
		   2, true, false, false, 0);

	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(tdb))
		goto out;
	ok1(tap_log_messages == 0);
	ok1(tdb_get_flags(tdb) & TDB_CANT_CHECK);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	/* We expect a warning! */
	ok1(tap_log_messages == 1);
	ok1(strstr(log_last, "capabilit"));
	ok1(tdb_summary(tdb, 0, &summary) == TDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	ok1(strstr(summary, "Capability 2 (uncheckable)\n"));
	free(summary);
	tdb_close(tdb);

	/* Capability says you can't write. */
	create_tdb("run-capabilities.tdb",
		   1, false, false, false,
		   2, false, true, false, 0);

	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	/* We expect a message. */
	ok1(!tdb);
	if (!ok1(tap_log_messages == 2))
		goto out;
	if (!ok1(strstr(log_last, "unknown")))
		goto out;
	ok1(strstr(log_last, "write"));

	/* We can open it read-only though! */
	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDONLY, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(tdb))
		goto out;
	ok1(tap_log_messages == 2);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	ok1(tap_log_messages == 2);
	ok1(tdb_summary(tdb, 0, &summary) == TDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	ok1(strstr(summary, "Capability 2 (read-only)\n"));
	free(summary);
	tdb_close(tdb);

	/* Capability says you can't open. */
	create_tdb("run-capabilities.tdb",
		   1, false, false, false,
		   2, false, false, true, 0);

	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	/* We expect a message. */
	ok1(!tdb);
	if (!ok1(tap_log_messages == 3))
		goto out;
	if (!ok1(strstr(log_last, "unknown")))
		goto out;

	/* Combine capabilities correctly. */
	create_tdb("run-capabilities.tdb",
		   1, false, false, false,
		   2, true, false, false,
		   3, false, true, false, 0);

	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	/* We expect a message. */
	ok1(!tdb);
	if (!ok1(tap_log_messages == 4))
		goto out;
	if (!ok1(strstr(log_last, "unknown")))
		goto out;
	ok1(strstr(log_last, "write"));

	/* We can open it read-only though! */
	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDONLY, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(tdb))
		goto out;
	ok1(tap_log_messages == 4);
	ok1(tdb_get_flags(tdb) & TDB_CANT_CHECK);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	/* We expect a warning! */
	ok1(tap_log_messages == 5);
	ok1(strstr(log_last, "unknown"));
	ok1(tdb_summary(tdb, 0, &summary) == TDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	ok1(strstr(summary, "Capability 2 (uncheckable)\n"));
	ok1(strstr(summary, "Capability 3 (read-only)\n"));
	free(summary);
	tdb_close(tdb);

	/* Two capability flags in one. */
	create_tdb("run-capabilities.tdb",
		   1, false, false, false,
		   2, true, true, false,
		   0);

	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	/* We expect a message. */
	ok1(!tdb);
	if (!ok1(tap_log_messages == 6))
		goto out;
	if (!ok1(strstr(log_last, "unknown")))
		goto out;
	ok1(strstr(log_last, "write"));

	/* We can open it read-only though! */
	failtest_suppress = false;
	tdb = tdb_open("run-capabilities.tdb", TDB_DEFAULT, O_RDONLY, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(tdb))
		goto out;
	ok1(tap_log_messages == 6);
	ok1(tdb_get_flags(tdb) & TDB_CANT_CHECK);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	/* We expect a warning! */
	ok1(tap_log_messages == 7);
	ok1(strstr(log_last, "unknown"));
	ok1(tdb_summary(tdb, 0, &summary) == TDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	ok1(strstr(summary, "Capability 2 (uncheckable,read-only)\n"));
	free(summary);
	tdb_close(tdb);

out:
	failtest_exit(exit_status());
}
