#include <ccan/failtest/failtest_override.h>
#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"
#include "layout.h"
#include "failtest_helper.h"
#include <stdarg.h>
#include "helprun-external-agent.h"

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

/* Creates a NTDB with various capabilities. */
static void create_ntdb(const char *name,
		       unsigned int cap,
		       bool breaks_check,
		       bool breaks_write,
		       bool breaks_open, ...)
{
	NTDB_DATA key, data;
	va_list ap;
	struct ntdb_layout *layout;
	struct ntdb_context *ntdb;
	int fd, clen;
	union ntdb_attribute seed_attr;

	/* Force a seed which doesn't allow records to clash! */
	seed_attr.base.attr = NTDB_ATTRIBUTE_SEED;
	seed_attr.base.next = &tap_log_attr;
	seed_attr.seed.seed = 0;

	key = ntdb_mkdata("Hello", 5);
	data = ntdb_mkdata("world", 5);

	/* Create a NTDB with some data, and some capabilities */
	layout = new_ntdb_layout();
	ntdb_layout_add_freetable(layout);
	ntdb_layout_add_used(layout, key, data, 6);
	clen = len_of(breaks_check, breaks_write, breaks_open);
	ntdb_layout_add_free(layout, 15496 - clen, 0);
	ntdb_layout_add_capability(layout, cap,
				   breaks_write, breaks_check, breaks_open,
				   clen);

	va_start(ap, breaks_open);
	while ((cap = va_arg(ap, int)) != 0) {
		breaks_check = va_arg(ap, int);
		breaks_write = va_arg(ap, int);
		breaks_open = va_arg(ap, int);

		key.dsize--;
		ntdb_layout_add_used(layout, key, data, 11 - key.dsize);
		clen = len_of(breaks_check, breaks_write, breaks_open);
		ntdb_layout_add_free(layout, 16304 - clen, 0);
		ntdb_layout_add_capability(layout, cap,
					  breaks_write, breaks_check,
					  breaks_open, clen);
	}
	va_end(ap);

	/* We open-code this, because we need to use the failtest write. */
	ntdb = ntdb_layout_get(layout, failtest_free, &seed_attr);

	fd = open(name, O_RDWR|O_TRUNC|O_CREAT, 0600);
	if (fd < 0)
		err(1, "opening %s for writing", name);
	if (write(fd, ntdb->file->map_ptr, ntdb->file->map_size)
	    != ntdb->file->map_size)
		err(1, "writing %s", name);
	close(fd);
	ntdb_close(ntdb);
	ntdb_layout_free(layout);
}

/* Note all the "goto out" early exits: they're to shorten failtest time. */
int main(int argc, char *argv[])
{
	struct ntdb_context *ntdb;
	char *summary;

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;
	plan_tests(60);

	failtest_suppress = true;
	/* Capability says you can ignore it? */
	create_ntdb("run-capabilities.ntdb", 1, false, false, false, 0);

	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDWR, 0,
			 &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(ntdb))
		goto out;
	ok1(tap_log_messages == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == NTDB_SUCCESS);
	ok1(tap_log_messages == 0);
	ntdb_close(ntdb);

	/* Two capabilitues say you can ignore them? */
	create_ntdb("run-capabilities.ntdb",
		   1, false, false, false,
		   2, false, false, false, 0);

	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDWR, 0,
			 &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(ntdb))
		goto out;
	ok1(tap_log_messages == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == NTDB_SUCCESS);
	ok1(tap_log_messages == 0);
	ok1(ntdb_summary(ntdb, 0, &summary) == NTDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	free(summary);
	ntdb_close(ntdb);

	/* Capability says you can't check. */
	create_ntdb("run-capabilities.ntdb",
		   1, false, false, false,
		   2, true, false, false, 0);

	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDWR, 0,
			 &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(ntdb))
		goto out;
	ok1(tap_log_messages == 0);
	ok1(ntdb_get_flags(ntdb) & NTDB_CANT_CHECK);
	ok1(ntdb_check(ntdb, NULL, NULL) == NTDB_SUCCESS);
	/* We expect a warning! */
	ok1(tap_log_messages == 1);
	ok1(strstr(log_last, "capabilit"));
	ok1(ntdb_summary(ntdb, 0, &summary) == NTDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	ok1(strstr(summary, "Capability 2 (uncheckable)\n"));
	free(summary);
	ntdb_close(ntdb);

	/* Capability says you can't write. */
	create_ntdb("run-capabilities.ntdb",
		   1, false, false, false,
		   2, false, true, false, 0);

	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDWR, 0,
			 &tap_log_attr);
	failtest_suppress = true;
	/* We expect a message. */
	ok1(!ntdb);
	if (!ok1(tap_log_messages == 2))
		goto out;
	if (!ok1(strstr(log_last, "unknown")))
		goto out;
	ok1(strstr(log_last, "write"));

	/* We can open it read-only though! */
	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDONLY, 0,
			 &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(ntdb))
		goto out;
	ok1(tap_log_messages == 2);
	ok1(ntdb_check(ntdb, NULL, NULL) == NTDB_SUCCESS);
	ok1(tap_log_messages == 2);
	ok1(ntdb_summary(ntdb, 0, &summary) == NTDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	ok1(strstr(summary, "Capability 2 (read-only)\n"));
	free(summary);
	ntdb_close(ntdb);

	/* Capability says you can't open. */
	create_ntdb("run-capabilities.ntdb",
		   1, false, false, false,
		   2, false, false, true, 0);

	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	/* We expect a message. */
	ok1(!ntdb);
	if (!ok1(tap_log_messages == 3))
		goto out;
	if (!ok1(strstr(log_last, "unknown")))
		goto out;

	/* Combine capabilities correctly. */
	create_ntdb("run-capabilities.ntdb",
		   1, false, false, false,
		   2, true, false, false,
		   3, false, true, false, 0);

	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	/* We expect a message. */
	ok1(!ntdb);
	if (!ok1(tap_log_messages == 4))
		goto out;
	if (!ok1(strstr(log_last, "unknown")))
		goto out;
	ok1(strstr(log_last, "write"));

	/* We can open it read-only though! */
	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDONLY, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(ntdb))
		goto out;
	ok1(tap_log_messages == 4);
	ok1(ntdb_get_flags(ntdb) & NTDB_CANT_CHECK);
	ok1(ntdb_check(ntdb, NULL, NULL) == NTDB_SUCCESS);
	/* We expect a warning! */
	ok1(tap_log_messages == 5);
	ok1(strstr(log_last, "unknown"));
	ok1(ntdb_summary(ntdb, 0, &summary) == NTDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	ok1(strstr(summary, "Capability 2 (uncheckable)\n"));
	ok1(strstr(summary, "Capability 3 (read-only)\n"));
	free(summary);
	ntdb_close(ntdb);

	/* Two capability flags in one. */
	create_ntdb("run-capabilities.ntdb",
		   1, false, false, false,
		   2, true, true, false,
		   0);

	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDWR, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	/* We expect a message. */
	ok1(!ntdb);
	if (!ok1(tap_log_messages == 6))
		goto out;
	if (!ok1(strstr(log_last, "unknown")))
		goto out;
	ok1(strstr(log_last, "write"));

	/* We can open it read-only though! */
	failtest_suppress = false;
	ntdb = ntdb_open("run-capabilities.ntdb", MAYBE_NOSYNC, O_RDONLY, 0,
		       &tap_log_attr);
	failtest_suppress = true;
	if (!ok1(ntdb))
		goto out;
	ok1(tap_log_messages == 6);
	ok1(ntdb_get_flags(ntdb) & NTDB_CANT_CHECK);
	ok1(ntdb_check(ntdb, NULL, NULL) == NTDB_SUCCESS);
	/* We expect a warning! */
	ok1(tap_log_messages == 7);
	ok1(strstr(log_last, "unknown"));
	ok1(ntdb_summary(ntdb, 0, &summary) == NTDB_SUCCESS);
	ok1(strstr(summary, "Capability 1\n"));
	ok1(strstr(summary, "Capability 2 (uncheckable,read-only)\n"));
	free(summary);
	ntdb_close(ntdb);

out:
	failtest_exit(exit_status());

	/*
	 * We will never reach this but the compiler complains if we do not
	 * return in this function.
	 */
	return EFAULT;
}
