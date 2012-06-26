#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <errno.h>

#define CCAN_RFC822_DEBUG

#include <ccan/rfc822/rfc822.h>

#include <ccan/rfc822/rfc822.c>

#include "testdata.h"

static void *failing_malloc(size_t size)
{
	return NULL;
}

static void abort_handler(int signum)
{
	ok(1, "Aborted");
	exit(0);
}

int main(int argc, char *argv[])
{
	const char *buf;
	size_t len;
	struct rfc822_msg *msg;
	struct sigaction sa = {
		.sa_handler = abort_handler,
	};
	int ret;

	plan_tests(2);

	ret = sigaction(SIGABRT, &sa, NULL);
	ok(ret, "Couldn't install signal handler: %s", strerror(errno));

	buf = assemble_msg(&test_msg_1, &len, 0);

	msg = rfc822_start(NULL, buf, len);

	talloc_set_allocator(failing_malloc, free, realloc);

	(void) rfc822_next_header(msg, NULL);

	ok(0, "Didn't get SIGABRT");

	rfc822_free(msg);
	talloc_free(buf);

	exit(exit_status());
}
