#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <errno.h>

#define CCAN_RFC822_DEBUG

#include <ccan/rfc822/rfc822.h>

#include <ccan/rfc822/rfc822.c>

#include "testdata.h"

static void mangle_list(struct rfc822_msg *msg)
{
	msg->headers.n.prev = NULL;
}

int main(int argc, char *argv[])
{
	void (*mangler)(struct rfc822_msg *msg);

	plan_tests(3 * 1);

	foreach_ptr(mangler, mangle_list) {
		const char *buf;
		size_t len;
		struct rfc822_msg *msg, *check;

		buf = assemble_msg(&test_msg_1, &len, 0);

		msg = rfc822_start(NULL, buf, len);
		fprintf(stderr, "msg = %p\n", msg);

		ok1(msg != NULL);

		(void) rfc822_next_header(msg, NULL);

		check = rfc822_check(msg, NULL);
		fprintf(stderr, "check = %p (1)\n", check);
		ok1(check == msg);

		mangler(msg);

		check = rfc822_check(msg, NULL);
		fprintf(stderr, "check = %p (2)\n", check);
		ok1(check == NULL);
	}

	exit(exit_status());
}
