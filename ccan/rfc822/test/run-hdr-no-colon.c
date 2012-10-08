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

#define NO_COLON_STR "This is a bad header\n"
const char no_colon_msg[] = 
	"Date: Tue, 22 Feb 2011 00:15:59 +1100\n"
	NO_COLON_STR
	"From: Mister From <from@example.com>\n"
	"To: Mizz To <to@example.org>\n"
	"Subject: Some subject\n"
	"Message-ID: <20110221131559.GA28327@example>\n";

static void test_no_colon(const char *buf, size_t len)
{
	struct rfc822_msg *msg;
	struct rfc822_header *hdr;
	struct bytestring hfull;

	plan_tests(6);

	msg = rfc822_start(NULL, buf, len);

	allocation_failure_check();

	hdr = rfc822_first_header(msg);
	allocation_failure_check();

	ok(hdr && (rfc822_header_errors(msg, hdr) == 0), "First header valid");
	allocation_failure_check();

	hdr = rfc822_next_header(msg, hdr);
	allocation_failure_check();

	ok(hdr && (rfc822_header_errors(msg, hdr) == RFC822_HDR_NO_COLON),
	   "Second header invalid");

	ok1(hdr && !rfc822_header_is(msg, hdr, NULL));
	ok1(hdr && !rfc822_header_is(msg, hdr, ""));
	ok1(hdr && !rfc822_header_is(msg, hdr, NO_COLON_STR));

	hfull = rfc822_header_raw_content(msg, hdr);
	allocation_failure_check();

	ok(bytestring_eq(hfull, BYTESTRING(NO_COLON_STR)),
		"Invalid header content");

	rfc822_free(msg);
	allocation_failure_check();
}

int main(int argc, char *argv[])
{
	failtest_setup(argc, argv);

	test_no_colon(no_colon_msg, sizeof(no_colon_msg));

	/* This exits depending on whether all tests passed */
	failtest_exit(exit_status());
}
