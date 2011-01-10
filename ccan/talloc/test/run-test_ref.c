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

/*
  test references 
*/
static bool test_ref1(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *ref, *r1;
	bool ret = false;

	root = talloc_named_const(ctx, 0, "root");
	if (!root)
		goto out;
	p1 = talloc_named_const(root, 1, "p1");
	if (!p1)
		goto out;
	p2 = talloc_named_const(p1, 1, "p2");
	if (!p2)
		goto out;
	if (!talloc_named_const(p1, 1, "x1"))
		goto out;
	if (!talloc_named_const(p1, 2, "x2"))
		goto out;
	if (!talloc_named_const(p1, 3, "x3"))
		goto out;

	r1 = talloc_named_const(root, 1, "r1");	
	if (!r1)
		goto out;
	ref = talloc_reference(r1, p2);
	if (!ref)
		goto out;

	CHECK_BLOCKS("ref1", p1, 5);
	CHECK_BLOCKS("ref1", p2, 1);
	CHECK_BLOCKS("ref1", r1, 2);

	talloc_free(p2);

	CHECK_BLOCKS("ref1", p1, 5);
	CHECK_BLOCKS("ref1", p2, 1);
	CHECK_BLOCKS("ref1", r1, 1);

	talloc_free(p1);

	CHECK_BLOCKS("ref1", r1, 1);

	talloc_free(r1);

	if (talloc_reference(root, NULL)) {
		return false;
	}

	CHECK_BLOCKS("ref1", root, 1);

	CHECK_SIZE("ref1", root, 0);
	ret = true;
out:
	talloc_free(root);
	return ret;
}

/*
  test references 
*/
static bool test_ref2(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *ref, *r1;
	bool ret = false;

	root = talloc_named_const(ctx, 0, "root");
	if (!root)
		goto out;
	p1 = talloc_named_const(root, 1, "p1");
	if (!p1)
		goto out;
	if (!talloc_named_const(p1, 1, "x1"))
		goto out;
	if (!talloc_named_const(p1, 1, "x2"))
		goto out;
	if (!talloc_named_const(p1, 1, "x3"))
		goto out;
	p2 = talloc_named_const(p1, 1, "p2");
	if (!p2)
		goto out;

	r1 = talloc_named_const(root, 1, "r1");	
	if (!r1)
		goto out;
	ref = talloc_reference(r1, p2);
	if (!ref)
		goto out;

	CHECK_BLOCKS("ref2", p1, 5);
	CHECK_BLOCKS("ref2", p2, 1);
	CHECK_BLOCKS("ref2", r1, 2);

	talloc_free(ref);

	CHECK_BLOCKS("ref2", p1, 5);
	CHECK_BLOCKS("ref2", p2, 1);
	CHECK_BLOCKS("ref2", r1, 1);

	talloc_free(p2);

	CHECK_BLOCKS("ref2", p1, 4);
	CHECK_BLOCKS("ref2", r1, 1);

	talloc_free(p1);

	CHECK_BLOCKS("ref2", r1, 1);

	talloc_free(r1);

	CHECK_SIZE("ref2", root, 0);
	ret = true;

out:
	talloc_free(root);
	return ret;
}

/*
  test references 
*/
static bool test_ref3(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *ref, *r1;
	bool ret = false;

	root = talloc_named_const(ctx, 0, "root");
	if (!root)
		goto out;
	p1 = talloc_named_const(root, 1, "p1");
	if (!p1)
		goto out;
	p2 = talloc_named_const(root, 1, "p2");
	if (!p2)
		goto out;
	r1 = talloc_named_const(p1, 1, "r1");
	if (!r1)
		goto out;
	ref = talloc_reference(p2, r1);
	if (!ref)
		goto out;

	CHECK_BLOCKS("ref3", p1, 2);
	CHECK_BLOCKS("ref3", p2, 2);
	CHECK_BLOCKS("ref3", r1, 1);

	talloc_free(p1);

	CHECK_BLOCKS("ref3", p2, 2);
	CHECK_BLOCKS("ref3", r1, 1);

	talloc_free(p2);

	CHECK_SIZE("ref3", root, 0);

	ret = true;
out:
	talloc_free(root);

	return ret;
}

/*
  test references 
*/
static bool test_ref4(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *ref, *r1;
	bool ret = false;

	root = talloc_named_const(ctx, 0, "root");
	if (!root)
		goto out;
	p1 = talloc_named_const(root, 1, "p1");
	if (!p1)
		goto out;
	if (!talloc_named_const(p1, 1, "x1"))
		goto out;
	if (!talloc_named_const(p1, 1, "x2"))
		goto out;
	if (!talloc_named_const(p1, 1, "x3"))
		goto out;
	p2 = talloc_named_const(p1, 1, "p2");
	if (!p2)
		goto out;

	r1 = talloc_named_const(root, 1, "r1");	
	if (!r1)
		goto out;
	ref = talloc_reference(r1, p2);
	if (!ref)
		goto out;

	CHECK_BLOCKS("ref4", p1, 5);
	CHECK_BLOCKS("ref4", p2, 1);
	CHECK_BLOCKS("ref4", r1, 2);

	talloc_free(r1);

	CHECK_BLOCKS("ref4", p1, 5);
	CHECK_BLOCKS("ref4", p2, 1);

	talloc_free(p2);

	CHECK_BLOCKS("ref4", p1, 4);

	talloc_free(p1);

	CHECK_SIZE("ref4", root, 0);

	ret = true;
out:
	talloc_free(root);

	return ret;
}

int main(int argc, char *argv[])
{
	plan_tests(34);
	failtest_init(argc, argv);

	talloc_enable_null_tracking();
	if (null_context) {
		ok1(test_ref1(NULL)
		    && test_ref2(NULL)
		    && test_ref3(NULL)
		    && test_ref4(NULL));
		/* This closes the leak, but don't free any other leaks! */
		ok1(!talloc_chunk_from_ptr(null_context)->child);
		talloc_disable_null_tracking();
	}
	failtest_exit(exit_status());
}
	
