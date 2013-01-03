#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <signal.h>
#include <errno.h>

#define CCAN_RFC822_DEBUG

#include <ccan/rfc822/rfc822.h>

#ifdef TAL_USE_TALLOC
#include <ccan/tal/talloc/talloc.h>
#else
#include <ccan/tal/tal.h>
#endif

static bool should_fail = false;

static void *mayfail_alloc(const void *ctx, size_t size)
{
	if (should_fail)
		return NULL;
	return tal_arrz(ctx, char, size);
}

/* Override various tallocation functions. */
#undef tal
#undef talz
#undef tal_arr
#define tal(ctx, type) mayfail_alloc((ctx), sizeof(type))
#define talz(ctx, type) mayfail_alloc((ctx), sizeof(type))
#define tal_arr(ctx, type, num) mayfail_alloc((ctx), sizeof(type)*(num))

#include <ccan/rfc822/rfc822.c>

#include "testdata.h"

static void abort_handler(int signum)
{
	printf("Aborted");
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

	ret = sigaction(SIGABRT, &sa, NULL);
	assert(ret == 0);

	buf = assemble_msg(&test_msg_1, &len, 0);

	msg = rfc822_start(NULL, buf, len);
	should_fail = true;
	(void) rfc822_next_header(msg, NULL);

	/* We should never get here! */
	abort();
}
