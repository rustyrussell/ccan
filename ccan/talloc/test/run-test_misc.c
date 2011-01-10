/* 
   Unix SMB/CIFS implementation.

   local testing of talloc routines.

   Copyright (C) Andrew Tridgell 2004
   Converted to ccan tests by Rusty Russell 2008
   
     ** NOTE! The following LGPL license applies to the talloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <ccan/failtest/failtest_override.h>
#include <ccan/talloc/talloc.c>
#include <stdbool.h>
#include <ccan/tap/tap.h>
#include <ccan/failtest/failtest.h>

#define torture_assert(test, expr, str)					\
	ok(expr, "%s [\n%s: Expression %s failed: %s\n]\n",		\
	   test, __location__, #expr, str)

#define torture_assert_str_equal(test, arg1, arg2, desc)	\
	ok(strcmp(arg1, arg2) == 0,				\
	   "%s [\n%s: Expected %s, got %s: %s\n]\n",		\
	   test, __location__, arg1, arg2, desc)

#define CHECK_SIZE(test, ptr, tsize)					\
	ok(talloc_total_size(ptr) == (tsize),				\
	   "%s [\nwrong '%s' tree size: got %u  expected %u\n]\n",	\
	   test, #ptr,							\
	   (unsigned)talloc_total_size(ptr),				\
	   (unsigned)tsize)

#define CHECK_BLOCKS(test, ptr, tblocks)				\
	ok(talloc_total_blocks(ptr) == (tblocks),			\
	   "%s [\nwrong '%s' tree blocks: got %u  expected %u\n]\n",	\
	   test, #ptr,							\
	   (unsigned)talloc_total_blocks(ptr),			\
	   (unsigned)tblocks)

#define CHECK_PARENT(test, ptr, parent)					\
	ok(talloc_parent(ptr) == (parent),				\
	   "%s [\n'%s' has wrong parent: got %p  expected %p\n]\n",	\
	   test, #ptr,							\
	   talloc_parent(ptr),						\
	   (parent))

struct torture_context;

static int fail_destructor(void *ptr)
{
	return -1;
}

/*
  miscellaneous tests to try to get a higher test coverage percentage
*/
static bool test_misc(const struct torture_context *ctx)
{
	void *root, *p1;
	char *p2;
	double *d;
	const char *name;
	bool ret = false;

	root = talloc_new(ctx);
	if (!root)
		goto out;

	p1 = talloc_size(root, 0x7fffffff);
	torture_assert("misc", !p1, "failed: large talloc allowed\n");

	p1 = talloc_strdup(root, "foo");
	if (!p1)
		goto out;

	if (talloc_increase_ref_count(p1) != 0)
		goto out;
	if (talloc_increase_ref_count(p1) != 0)
		goto out;
	if (talloc_increase_ref_count(p1) != 0)
		goto out;
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	talloc_free(p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	talloc_unlink(NULL, p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	p2 = talloc_strdup(p1, "foo");
	if (!p2)
		goto out;
	torture_assert("misc", talloc_unlink(root, p2) == -1,
				   "failed: talloc_unlink() of non-reference context should return -1\n");
	torture_assert("misc", talloc_unlink(p1, p2) == 0,
		"failed: talloc_unlink() of parent should succeed\n");
	talloc_free(p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);

	name = talloc_set_name(p1, "my name is %s", "foo");
	if (!name)
		goto out;
	torture_assert_str_equal("misc", talloc_get_name(p1), "my name is foo",
		"failed: wrong name after talloc_set_name(my name is foo)");
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);

	talloc_set_name_const(p1, NULL);
	torture_assert_str_equal ("misc", talloc_get_name(p1), "UNNAMED",
		"failed: wrong name after talloc_set_name(NULL)");
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);

	torture_assert("misc", talloc_free(NULL) == -1, 
				   "talloc_free(NULL) should give -1\n");

	talloc_set_destructor(p1, fail_destructor);
	torture_assert("misc", talloc_free(p1) == -1, 
		"Failed destructor should cause talloc_free to fail\n");
	talloc_set_destructor(p1, NULL);

	p2 = (char *)talloc_zero_size(p1, 20);
	if (!p2)
		goto out;
	torture_assert("misc", p2[19] == 0, "Failed to give zero memory\n");
	talloc_free(p2);

	torture_assert("misc", talloc_strdup(root, NULL) == NULL,
		"failed: strdup on NULL should give NULL\n");

	p2 = talloc_strndup(p1, "foo", 2);
	if (!p2)
		goto out;
	torture_assert("misc", strcmp("fo", p2) == 0, 
				   "strndup doesn't work\n");
	p2 = talloc_asprintf_append(p2, "o%c", 'd');
	if (!p2)
		goto out;
	torture_assert("misc", strcmp("food", p2) == 0, 
				   "talloc_asprintf_append doesn't work\n");
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 3);

	p2 = talloc_asprintf_append(NULL, "hello %s", "world");
	if (!p2)
		goto out;
	torture_assert("misc", strcmp("hello world", p2) == 0,
		"talloc_asprintf_append doesn't work\n");
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 3);
	talloc_free(p2);

	d = talloc_array(p1, double, 0x20000000);
	torture_assert("misc", !d, "failed: integer overflow not detected\n");

	d = talloc_realloc(p1, d, double, 0x20000000);
	torture_assert("misc", !d, "failed: integer overflow not detected\n");

	talloc_free(p1);
	CHECK_BLOCKS("misc", root, 1);

	p1 = talloc_named(root, 100, "%d bytes", 100);
	if (!p1)
		goto out;
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);
	talloc_unlink(root, p1);

	p1 = talloc_init("%d bytes", 200);
	if (!p1)
		goto out;
	p2 = talloc_asprintf(p1, "my test '%s'", "string");
	if (!p2)
		goto out;
	torture_assert_str_equal("misc", p2, "my test 'string'",
		"failed: talloc_asprintf(\"my test '%%s'\", \"string\") gave: \"%s\"");
	CHECK_BLOCKS("misc", p1, 3);
	CHECK_SIZE("misc", p2, 17);
	CHECK_BLOCKS("misc", root, 1);
	talloc_unlink(NULL, p1);

	p1 = talloc_named_const(root, 10, "p1");
	if (!p1)
		goto out;
	p2 = (char *)talloc_named_const(root, 20, "p2");
	if (!p2)
		goto out;
	if (!talloc_reference(p1, p2))
		goto out;
	talloc_unlink(root, p2);
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);
	talloc_unlink(p1, p2);
	talloc_unlink(root, p1);

	p1 = talloc_named_const(root, 10, "p1");
	if (!p1)
		goto out;
	p2 = (char *)talloc_named_const(root, 20, "p2");
	if (!p2)
		goto out;
	if (!talloc_reference(NULL, p2))
		goto out;
	talloc_unlink(root, p2);
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	talloc_unlink(NULL, p2);
	talloc_unlink(root, p1);

	/* Test that talloc_unlink is a no-op */

	torture_assert("misc", talloc_unlink(root, NULL) == -1,
		"failed: talloc_unlink(root, NULL) == -1\n");

	CHECK_SIZE("misc", root, 0);

	talloc_enable_leak_report();
	talloc_enable_leak_report_full();
	ret = true;
out:
	talloc_free(root);
	return ret;
}

int main(int argc, char *argv[])
{
	plan_tests(47);
	failtest_init(argc, argv);

	talloc_enable_null_tracking();
	if (null_context) {
		ok1(test_misc(NULL));
		/* This closes the leak, but don't free any other leaks! */
		ok1(!talloc_chunk_from_ptr(null_context)->child);
		talloc_disable_null_tracking();
	}
	failtest_exit(exit_status());
}
