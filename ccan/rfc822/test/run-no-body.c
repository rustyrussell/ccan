#include <ccan/foreach/foreach.h>
#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <string.h>

#define CCAN_RFC822_DEBUG

#include <ccan/rfc822/rfc822.h>

#include <ccan/rfc822/rfc822.c>

#include "testdata.h"
#include "helper.h"

const char no_body[] = 
	"Date: Tue, 22 Feb 2011 00:15:59 +1100\n"
	"From: Mister From <from@example.com>\n"
	"To: Mizz To <to@example.org>\n"
	"Subject: Some subject\n"
	"Message-ID: <20110221131559.GA28327@example>\n";

const char truncated[] = 
	"Date: Tue, 22 Feb 2011 00:15:59 +1100\n"
	"From: Mister From <from@example.com>\n"
	"To: Mizz To <to@";

static int test_no_body(const char *buf, size_t len)
{
	struct rfc822_msg *msg;
	struct bytestring body;
	int ok = 1;

	msg = rfc822_start(NULL, buf, len);
	allocation_failure_check();

	body = rfc822_body(msg);
	allocation_failure_check();
	if (body.ptr)
		ok = 0;

	rfc822_free(msg);
	allocation_failure_check();
	return ok;
}

static int test_truncated(const char *buf, size_t len)
{
	struct rfc822_msg *msg;
	struct rfc822_header *h = NULL;
	struct bytestring body;
	int ok = 1;

	msg = rfc822_start(NULL, buf, len);
	allocation_failure_check();

	do {
		h = rfc822_next_header(msg, h);
		allocation_failure_check();
	} while (h);

	body = rfc822_body(msg);
	allocation_failure_check();
	if (body.ptr)
		ok = 0;

	rfc822_free(msg);
	allocation_failure_check();
	return ok;
}

int main(int argc, char *argv[])
{
	failtest_setup(argc, argv);

	/* This is how many tests you plan to run */
	plan_tests(3);

	ok1(test_no_body(no_body, sizeof(no_body)));
	ok1(test_no_body(truncated, sizeof(truncated)));
	ok1(test_truncated(truncated, sizeof(truncated)));

	/* This exits depending on whether all tests passed */
	failtest_exit(exit_status());
}
