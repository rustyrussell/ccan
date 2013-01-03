#include <stdlib.h>
#include <stdio.h>

#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>

#include <ccan/rfc822/rfc822.h>

#include "helper.h"

/* failtest limitations mean we need these wrappers to test talloc
 * failure paths. */
#ifndef TAL_USE_TALLOC
static void *malloc_wrapper(size_t size)
{
	return malloc(size);
}

static void free_wrapper(void *ptr)
{
	free(ptr);
}

static void *realloc_wrapper(void *ptr, size_t size)
{
	return realloc(ptr, size);
}
#endif

#if 0
static void allocation_failure_exit(const char *s)
{
	fprintf(stderr, "Allocation failure: %s", s);
	exit(0);
}
#endif

static bool allocation_failed = false;

static void allocation_failure_continue(const char *s)
{
	fprintf(stderr, "Allocation failure: %s", s);
	allocation_failed = true;
}

void allocation_failure_check(void)
{
	if (allocation_failed) {
		fprintf(stderr, "Exiting due to earlier failed allocation\n");
		exit(0);
	}
}

#ifdef TAL_USE_TALLOC
#include <ccan/tal/talloc/talloc.h>
#else
#include <ccan/tal/tal.h>
#endif

/* Don't abort on allocation failures! */
static void noabort_wrapper(const char *why)
{
	return;
}

void failtest_setup(int argc, char *argv[])
{
	failtest_init(argc, argv);
	rfc822_set_allocation_failure_handler(allocation_failure_continue);
#ifdef TAL_USE_TALLOC
	/* FIXME: we can't inject allocation failures in talloc! */
	tal_set_backend(NULL, NULL, NULL, noabort_wrapper);
#else
	tal_set_backend(malloc_wrapper, realloc_wrapper, free_wrapper,
			noabort_wrapper);
#endif
}

void check_header(struct rfc822_msg *msg,
		  struct rfc822_header *h,
		  const char *name, const char *val,
		  enum rfc822_header_errors experr, int crlf)
{
	enum rfc822_header_errors errs;
	struct bytestring hname, hvalue, hfull;
	size_t namelen = strlen(name);
	size_t valuelen = strlen(val);
	size_t nln = crlf ? 2 : 1;
	size_t fulllen = namelen + valuelen + 1 + nln;

	errs = rfc822_header_errors(msg, h);
	ok(errs == experr, "Header errors 0x%x != 0x%x", errs, experr);
	allocation_failure_check();

	hname = rfc822_header_raw_name(msg, h);
	allocation_failure_check();

	ok(hname.ptr && bytestring_eq(hname, bytestring_from_string(name)),
	   "Header name \"%.*s\"", (int)hname.len, hname.ptr);

	hvalue = rfc822_header_raw_value(msg, h);
	allocation_failure_check();

	ok(hvalue.ptr && ((valuelen + nln) == hvalue.len)
	   && (memcmp(val, hvalue.ptr, valuelen) == 0)
	   && (!crlf || (hvalue.ptr[hvalue.len - 2] == '\r'))
	   && (hvalue.ptr[hvalue.len - 1] == '\n'),
	   "Header value");

	hfull = rfc822_header_raw_content(msg, h);
	allocation_failure_check();

	ok(hfull.ptr && (fulllen == hfull.len)
	   && (memcmp(name, hfull.ptr, namelen) == 0)
	   && (hfull.ptr[namelen] == ':')
	   && (memcmp(val, hfull.ptr + namelen + 1, valuelen) == 0)
	   && (!crlf || (hfull.ptr[fulllen-2] == '\r'))
	   && (hfull.ptr[fulllen-1] == '\n'),
	   "Full header");
}
