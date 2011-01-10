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
	   (unsigned)talloc_total_blocks(ptr),				\
	   (unsigned)tblocks)

#define CHECK_PARENT(test, ptr, parent)					\
	ok(talloc_parent(ptr) == (parent),				\
	   "%s [\n'%s' has wrong parent: got %p  expected %p\n]\n",	\
	   test, #ptr,							\
	   talloc_parent(ptr),						\
	   (parent))

struct torture_context;

static bool test_unref_reparent(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *c1;
	bool ret = false;

	root = talloc_named_const(ctx, 0, "root");
	if (!root)
		goto out;
	p1 = talloc_named_const(root, 1, "orig parent");
	if (!p1)
		goto out;
	p2 = talloc_named_const(root, 1, "parent by reference");
	if (!p2)
		goto out;

	c1 = talloc_named_const(p1, 1, "child");
	if (!c1)
		goto out;

	if (!talloc_reference(p2, c1))
		goto out;

	CHECK_PARENT("unref_reparent", c1, p1);

	talloc_free(p1);

	CHECK_PARENT("unref_reparent", c1, p2);

	talloc_unlink(p2, c1);

	CHECK_SIZE("unref_reparent", root, 1);

	talloc_free(p2);
	ret = true;
out:
 	talloc_free(root);

	return ret;
}

static bool test_lifeless(const struct torture_context *ctx)
{
	void *top = talloc_new(ctx);
	char *parent, *child; 
	void *child_owner = talloc_new(ctx);

	parent = talloc_strdup(top, "parent");
	if (!parent)
		return false;
	child = talloc_strdup(parent, "child");  
	if (!child) {
		talloc_free(parent);
		return false;
	}
	if (!talloc_reference(child, parent)) {
		talloc_free(parent);
		return false;
	}

	if (!talloc_reference(child_owner, child)) {
		talloc_unlink(child, parent);
		talloc_free(parent);
		return false;
	}

	talloc_unlink(top, parent);
	talloc_free(child);
	talloc_free(top);
	talloc_free(child_owner);
	talloc_free(child);

	return true;
}

int main(int argc, char *argv[])
{
	plan_tests(5);
	failtest_init(argc, argv);

	talloc_enable_null_tracking();
	if (null_context) {
		ok1(test_unref_reparent(NULL) && test_lifeless(NULL));
		/* This closes the leak, but don't free any other leaks! */
		ok1(!talloc_chunk_from_ptr(null_context)->child);
		talloc_disable_null_tracking();
	}
	failtest_exit(exit_status());
}
