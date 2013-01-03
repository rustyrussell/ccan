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

#define CHECK_HEADERS(_e, _msg, _h, _n, _crlf)	\
	do { \
		int _i; \
		for (_i = 0; _i < (_e)->nhdrs; _i++) {	\
			(_h) = rfc822_next_header((_msg), (_h)); \
			ok((_h), "header %d exists %s", _i, (_n)); \
			if (!(_h)) \
				break; \
			check_header((_msg), (_h), (_e)->hdrs[_i].name,	\
				     (_e)->hdrs[_i].val,		\
				     (_e)->hdrs[_i].errors, crlf);	\
		} \
	} while (0)

static void test_bodyhdr(const struct aexample *e, const char *buf, size_t len,
			 const char *exname, int crlf)
{
	struct rfc822_msg *msg;
	struct rfc822_header *h = NULL;
	struct bytestring body;

	msg = rfc822_start(NULL, buf, len);
	allocation_failure_check();

	ok(msg, "opened %s", exname);

	body = rfc822_body(msg);
	allocation_failure_check();
	ok(bytestring_eq(body, bytestring_from_string(e->body)),
	   "body content %s", exname);

	CHECK_HEADERS(e, msg, h, exname, crlf);
	h = rfc822_next_header(msg, h);
	allocation_failure_check();
	ok(!h, "Too many headers for %s", exname);

	rfc822_free(msg);
	allocation_failure_check();
}

static void test_hdrbody(const struct aexample *e, const char *buf, size_t len,
			 const char *exname, int crlf)
{
	struct rfc822_msg *msg;
	struct rfc822_header *h = NULL;
	struct bytestring body;

	msg = rfc822_start(NULL, buf, len);
	allocation_failure_check();
	ok(msg, "opened %s", exname);

	CHECK_HEADERS(e, msg, h, exname, crlf);
	h = rfc822_next_header(msg, h);
	allocation_failure_check();
	ok(!h, "Too many headers for %s", exname);

	body = rfc822_body(msg);
	allocation_failure_check();
	ok(bytestring_eq(body, bytestring_from_string(e->body)),
	   "body content %s", exname);

	rfc822_free(msg);
	allocation_failure_check();
}

static void test_hdrhdr(const struct aexample *e, const char *buf, size_t len,
			const char *exname, int crlf)
{
	struct rfc822_msg *msg;
	struct rfc822_header *h;

	msg = rfc822_start(NULL, buf, len);
	allocation_failure_check();
	ok(msg, "opened %s", exname);

	h = NULL;
	CHECK_HEADERS(e, msg, h, exname, crlf);

	h = rfc822_next_header(msg, h);
	allocation_failure_check();
	ok(!h, "Too many headers for %s", exname);

	/* And again, this time it should be cached */
	h = NULL;
	CHECK_HEADERS(e, msg, h, exname, crlf);

	h = rfc822_next_header(msg, h);
	allocation_failure_check();
	ok(!h, "Too many headers for %s", exname);

	rfc822_free(msg);
	allocation_failure_check();
}

int main(int argc, char *argv[])
{
	struct aexample *e;

	/* This is how many tests you plan to run */
	plan_tests(20*num_aexamples()
		   + (36 + CHECK_HEADER_NUMTESTS)*num_aexample_hdrs());

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

			test_bodyhdr(e, buf, len, exname, crlf);
			test_hdrbody(e, buf, len, exname, crlf);
			test_hdrhdr(e, buf, len, exname, crlf);

			tal_free(buf);
		}
	}

	/* This exits depending on whether all tests passed */
	failtest_exit(exit_status());
}
