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

#define NAME_TEMPLATE		"X-Bad-Header"
#define NAME_TEMPLATE_LEN	(strlen(NAME_TEMPLATE))

const char bad_name_template[] =
	NAME_TEMPLATE ": This is a good header that will become bad\n";

static bool bad_name(const char *buf, char c)
{
	struct rfc822_msg *msg;
	struct rfc822_header *hdr;
	enum rfc822_header_errors errs;
	struct bytestring hname;

	msg = rfc822_start(NULL, buf, strlen(buf));

	allocation_failure_check();

	hdr = rfc822_first_header(msg);
	ok1(hdr);

	allocation_failure_check();

	errs = rfc822_header_errors(msg, hdr);

	allocation_failure_check();

	ok1(!(errs & ~RFC822_HDR_BAD_NAME_CHARS));

	/* Check raw_name still works properly */
	hname = rfc822_header_raw_name(msg, hdr);

	allocation_failure_check();

	ok1(hname.ptr && hname.len == NAME_TEMPLATE_LEN);
	ok1((hname.ptr[0] == c) &&
	    (memcmp(hname.ptr + 1, NAME_TEMPLATE + 1, NAME_TEMPLATE_LEN - 1) == 0));

	rfc822_free(msg);

	allocation_failure_check();

	return !!(errs & RFC822_HDR_BAD_NAME_CHARS);
}

int main(int argc, char *argv[])
{
	char c;

	failtest_setup(argc, argv);

	plan_tests(5 * (1 + 3 + 4));

	ok1(!bad_name(bad_name_template, bad_name_template[0]));

	/* Good characters */
	foreach_int(c, 'a', 'Z', '3') {
		char *tmp = strdup(bad_name_template);

		tmp[0] = c;

		ok1(!bad_name(tmp, c));

		free(tmp);
	}

	/* Bad characters */
	foreach_int(c, ' ', '\t', '\b', '\x80') {
		char *tmp = strdup(bad_name_template);

		tmp[0] = c;

		ok1(bad_name(tmp, c));

		free(tmp);
	}

	/* This exits depending on whether all tests passed */
	failtest_exit(exit_status());
}
