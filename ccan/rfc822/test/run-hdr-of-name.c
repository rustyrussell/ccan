#include <ccan/foreach/foreach.h>
#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>
#include <stdlib.h>
#include <string.h>

#define CCAN_RFC822_DEBUG

#include <ccan/rfc822/rfc822.h>

#include <ccan/rfc822/rfc822.c>

#include "testdata.h"
#include "helper.h"

static void test_hdrbyname(const struct aexample *e, const char *buf, size_t len,
			   const char *exname, int crlf)
{
	struct rfc822_msg *msg;
	struct rfc822_header *h, *hx;
	int i, j;

	msg = rfc822_start(NULL, buf, len);
	allocation_failure_check();
	ok(msg, "opened %s", exname);

	for (i = 0; i < e->nhdrs; i++) {
		struct testhdr *eh = &e->hdrs[i];

		h = rfc822_first_header_of_name(msg, eh->name);
		hx = rfc822_next_header_of_name(msg, NULL, eh->name);
		ok1(h == hx);

		for (j = 0; h && j < eh->index; j++)
			h = rfc822_next_header_of_name(msg, h, eh->name);
		ok(h, "header \"%s\" (#%d) exists", eh->name, eh->index);
		if (!h)
			break;
		check_header(msg, h, eh->name, eh->val, eh->errors, crlf);

		h = rfc822_next_header_of_name(msg, h, eh->name);
		ok1((eh->index != eh->last) ^ !h);
	}

	h = rfc822_first_header_of_name(msg, NULL);
	ok(!h, "Never match NULL name");

	rfc822_free(msg);
	allocation_failure_check();
}

int main(int argc, char *argv[])
{
	struct aexample *e;

	/* This is how many tests you plan to run */
	plan_tests(6*num_aexamples() + 14*num_aexample_hdrs());

	failtest_setup(argc, argv);

	for_each_aexample(e) {
		int crlf;

		foreach_int(crlf, 0, 1) {
			const char *buf;
			size_t len;
			char exname[256];

			sprintf(exname, "%s[%s]", e->name, NLT(crlf));

			buf = assemble_msg(e, &len, crlf);
			ok((buf), "assembled %s", exname);
			if (!buf)
				continue;

			test_hdrbyname(e, buf, len, exname, crlf);

			tal_free(buf);
		}
	}

	/* This exits depending on whether all tests passed */
	failtest_exit(exit_status());
}
