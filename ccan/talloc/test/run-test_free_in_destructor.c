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

static bool test_talloc_free_in_destructor_run;
static int _test_talloc_free_in_destructor(void **ptr)
{
	talloc_free(*ptr);
	test_talloc_free_in_destructor_run = true;
	return 0;
}

static bool test_talloc_free_in_destructor(const struct torture_context *ctx)
{
	void *level0;
	void *level1;
	void *level2;
	void *level3;
	void *level4;
	void **level5;
	bool ret = false;

	level0 = talloc_new(ctx);
	if (!level0)
		goto out;
	level1 = talloc_new(level0);
	if (!level1)
		goto out;
	level2 = talloc_new(level1);
	if (!level2)
		goto out;
	level3 = talloc_new(level2);
	if (!level3)
		goto out;
	level4 = talloc_new(level3);
	if (!level4)
		goto out;
	level5 = talloc(level4, void *);
	if (!level5)
		goto out;

	*level5 = talloc_reference(NULL, level3);

	test_talloc_free_in_destructor_run = false;
	talloc_set_destructor(level5, _test_talloc_free_in_destructor);

	talloc_free(level1);
	ret = true;
out:
	talloc_free(level0);

	ok1(test_talloc_free_in_destructor_run);

	return ret;
}

int main(int argc, char *argv[])
{
	plan_tests(3);
	failtest_init(argc, argv);

	talloc_enable_null_tracking();
	if (null_context) {
		ok1(test_talloc_free_in_destructor(NULL));
		/* This closes the leak, but don't free any other leaks! */
		ok1(!talloc_chunk_from_ptr(null_context)->child);
		talloc_disable_null_tracking();
	}
	failtest_exit(exit_status());
}
	
