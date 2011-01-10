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
  test realloc
*/
static bool test_realloc(const struct torture_context *ctx)
{
	void *root, *p1, *p2;
	bool ret = false;

	root = talloc_new(ctx);
	if (!root)
		goto out;

	p1 = talloc_size(root, 10);
	if (!p1)
		goto out;
	CHECK_SIZE("realloc", p1, 10);

	p1 = talloc_realloc_size(NULL, p1, 20);
	if (!p1)
		goto out;
	CHECK_SIZE("realloc", p1, 20);

	if (!talloc_new(p1))
		goto out;

	p2 = talloc_realloc_size(p1, NULL, 30);
	if (!p2)
		goto out;

	if (!talloc_new(p1))
		goto out;

	p2 = talloc_realloc_size(p1, p2, 40);
	if (!p2)
		goto out;

	CHECK_SIZE("realloc", p2, 40);
	CHECK_SIZE("realloc", root, 60);
	CHECK_BLOCKS("realloc", p1, 4);

	p1 = talloc_realloc_size(NULL, p1, 20);
	if (!p1)
		goto out;
	CHECK_SIZE("realloc", p1, 60);

	if (talloc_increase_ref_count(p2) != 0)
		goto out;
	torture_assert("realloc", talloc_realloc_size(NULL, p2, 5) == NULL,
		"failed: talloc_realloc() on a referenced pointer should fail\n");
	CHECK_BLOCKS("realloc", p1, 4);

	ok1(talloc_realloc_size(NULL, p2, 0) == NULL);
	ok1(talloc_realloc_size(NULL, p2, 0) == NULL);
	CHECK_BLOCKS("realloc", p1, 3);

	torture_assert("realloc", talloc_realloc_size(NULL, p1, 0x7fffffff) == NULL,
		"failed: oversize talloc should fail\n");

	ok1(talloc_realloc_size(NULL, p1, 0) == NULL);

	CHECK_BLOCKS("realloc", root, 1);
	CHECK_SIZE("realloc", root, 0);
	ret = true;
	
out:
	talloc_free(root);

	return ret;
}

/*
  test realloc with a child
*/
static bool test_realloc_child(const struct torture_context *ctx)
{
	void *root;
	struct el2 {
		const char *name;
	} *el2;	
	struct el1 {
		int count;
		struct el2 **list, **list2, **list3;
	} *el1;
	bool ret = false;

	root = talloc_new(ctx);
	if (!root)
		goto out;

	el1 = talloc(root, struct el1);
	if (!el1)
		goto out;
	el1->list = talloc(el1, struct el2 *);
	if (!el1->list)
		goto out;
	el1->list[0] = talloc(el1->list, struct el2);
	if (!el1->list[0])
		goto out;
	el1->list[0]->name = talloc_strdup(el1->list[0], "testing");
	if (!el1->list[0]->name)
		goto out;

	el1->list2 = talloc(el1, struct el2 *);
	if (!el1->list2)
		goto out;
	el1->list2[0] = talloc(el1->list2, struct el2);
	if (!el1->list2[0])
		goto out;
	el1->list2[0]->name = talloc_strdup(el1->list2[0], "testing2");
	if (!el1->list2[0]->name)
		goto out;

	el1->list3 = talloc(el1, struct el2 *);
	if (!el1->list3)
		goto out;
	el1->list3[0] = talloc(el1->list3, struct el2);
	if (!el1->list3[0])
		goto out;
	el1->list3[0]->name = talloc_strdup(el1->list3[0], "testing2");
	if (!el1->list3[0]->name)
		goto out;
	
	el2 = talloc(el1->list, struct el2);
	if (!el2)
		goto out;
	el2 = talloc(el1->list2, struct el2);
	if (!el2)
		goto out;
	el2 = talloc(el1->list3, struct el2);
	if (!el2)
		goto out;

	el1->list = talloc_realloc(el1, el1->list, struct el2 *, 100);
	if (!el1->list)
		goto out;
	el1->list2 = talloc_realloc(el1, el1->list2, struct el2 *, 200);
	if (!el1->list2)
		goto out;
	el1->list3 = talloc_realloc(el1, el1->list3, struct el2 *, 300);
	if (!el1->list3)
		goto out;

	ret = true;
out:
	talloc_free(root);

	return ret;
}

int main(int argc, char *argv[])
{
	plan_tests(17);
	failtest_init(argc, argv);

	talloc_enable_null_tracking();
	if (null_context) {
		ok1(test_realloc(NULL) && test_realloc_child(NULL));
		/* This closes the leak, but don't free any other leaks! */
		ok1(!talloc_chunk_from_ptr(null_context)->child);
		talloc_disable_null_tracking();
	}
	failtest_exit(exit_status());
}
	
