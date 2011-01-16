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

#include <ccan/talloc/talloc.c>
#include <stdbool.h>
#include <ccan/tap/tap.h>

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

	root = talloc_named_const(ctx, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	p2 = talloc_named_const(p1, 1, "p2");
	talloc_named_const(p1, 1, "x1");
	talloc_named_const(p1, 2, "x2");
	talloc_named_const(p1, 3, "x3");

	r1 = talloc_named_const(root, 1, "r1");	
	ref = talloc_reference(r1, p2);

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

	talloc_free(root);
	return true;
}

/*
  test references 
*/
static bool test_ref2(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *ref, *r1;

	root = talloc_named_const(ctx, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	talloc_named_const(p1, 1, "x1");
	talloc_named_const(p1, 1, "x2");
	talloc_named_const(p1, 1, "x3");
	p2 = talloc_named_const(p1, 1, "p2");

	r1 = talloc_named_const(root, 1, "r1");	
	ref = talloc_reference(r1, p2);

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

	talloc_free(root);
	return true;
}

/*
  test references 
*/
static bool test_ref3(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *ref, *r1;

	root = talloc_named_const(ctx, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	p2 = talloc_named_const(root, 1, "p2");
	r1 = talloc_named_const(p1, 1, "r1");
	ref = talloc_reference(p2, r1);

	CHECK_BLOCKS("ref3", p1, 2);
	CHECK_BLOCKS("ref3", p2, 2);
	CHECK_BLOCKS("ref3", r1, 1);

	talloc_free(p1);

	CHECK_BLOCKS("ref3", p2, 2);
	CHECK_BLOCKS("ref3", r1, 1);

	talloc_free(p2);

	CHECK_SIZE("ref3", root, 0);

	talloc_free(root);

	return true;
}

/*
  test references 
*/
static bool test_ref4(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *ref, *r1;

	root = talloc_named_const(ctx, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	talloc_named_const(p1, 1, "x1");
	talloc_named_const(p1, 1, "x2");
	talloc_named_const(p1, 1, "x3");
	p2 = talloc_named_const(p1, 1, "p2");

	r1 = talloc_named_const(root, 1, "r1");	
	ref = talloc_reference(r1, p2);

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

	talloc_free(root);

	return true;
}


/*
  test references 
*/
static bool test_unlink1(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *ref, *r1;

	root = talloc_named_const(ctx, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	talloc_named_const(p1, 1, "x1");
	talloc_named_const(p1, 1, "x2");
	talloc_named_const(p1, 1, "x3");
	p2 = talloc_named_const(p1, 1, "p2");

	r1 = talloc_named_const(p1, 1, "r1");	
	ref = talloc_reference(r1, p2);

	CHECK_BLOCKS("unlink", p1, 7);
	CHECK_BLOCKS("unlink", p2, 1);
	CHECK_BLOCKS("unlink", r1, 2);

	talloc_unlink(r1, p2);

	CHECK_BLOCKS("unlink", p1, 6);
	CHECK_BLOCKS("unlink", p2, 1);
	CHECK_BLOCKS("unlink", r1, 1);

	talloc_free(p1);

	CHECK_SIZE("unlink", root, 0);

	talloc_free(root);

	return true;
}

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

	root = talloc_new(ctx);

	p1 = talloc_size(root, 0x7fffffff);
	torture_assert("misc", !p1, "failed: large talloc allowed\n");

	p1 = talloc_strdup(root, "foo");
	talloc_increase_ref_count(p1);
	talloc_increase_ref_count(p1);
	talloc_increase_ref_count(p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	talloc_free(p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	talloc_unlink(NULL, p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	p2 = talloc_strdup(p1, "foo");
	torture_assert("misc", talloc_unlink(root, p2) == -1,
				   "failed: talloc_unlink() of non-reference context should return -1\n");
	torture_assert("misc", talloc_unlink(p1, p2) == 0,
		"failed: talloc_unlink() of parent should succeed\n");
	talloc_free(p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);

	name = talloc_set_name(p1, "my name is %s", "foo");
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
	torture_assert("misc", p2[19] == 0, "Failed to give zero memory\n");
	talloc_free(p2);

	torture_assert("misc", talloc_strdup(root, NULL) == NULL,
		"failed: strdup on NULL should give NULL\n");

	p2 = talloc_strndup(p1, "foo", 2);
	torture_assert("misc", strcmp("fo", p2) == 0, 
				   "strndup doesn't work\n");
	p2 = talloc_asprintf_append(p2, "o%c", 'd');
	torture_assert("misc", strcmp("food", p2) == 0, 
				   "talloc_asprintf_append doesn't work\n");
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 3);

	p2 = talloc_asprintf_append(NULL, "hello %s", "world");
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
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);
	talloc_unlink(root, p1);

	p1 = talloc_init("%d bytes", 200);
	p2 = talloc_asprintf(p1, "my test '%s'", "string");
	torture_assert_str_equal("misc", p2, "my test 'string'",
		"failed: talloc_asprintf(\"my test '%%s'\", \"string\") gave: \"%s\"");
	CHECK_BLOCKS("misc", p1, 3);
	CHECK_SIZE("misc", p2, 17);
	CHECK_BLOCKS("misc", root, 1);
	talloc_unlink(NULL, p1);

	p1 = talloc_named_const(root, 10, "p1");
	p2 = (char *)talloc_named_const(root, 20, "p2");
	(void)talloc_reference(p1, p2);
	talloc_unlink(root, p2);
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);
	talloc_unlink(p1, p2);
	talloc_unlink(root, p1);

	p1 = talloc_named_const(root, 10, "p1");
	p2 = (char *)talloc_named_const(root, 20, "p2");
	(void)talloc_reference(NULL, p2);
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

	talloc_free(root);

	CHECK_SIZE("misc", NULL, 0);

	talloc_enable_leak_report();
	talloc_enable_leak_report_full();

	return true;
}


/*
  test realloc
*/
static bool test_realloc(const struct torture_context *ctx)
{
	void *root, *p1, *p2;

	root = talloc_new(ctx);

	p1 = talloc_size(root, 10);
	CHECK_SIZE("realloc", p1, 10);

	p1 = talloc_realloc_size(NULL, p1, 20);
	CHECK_SIZE("realloc", p1, 20);

	talloc_new(p1);

	p2 = talloc_realloc_size(p1, NULL, 30);

	talloc_new(p1);

	p2 = talloc_realloc_size(p1, p2, 40);

	CHECK_SIZE("realloc", p2, 40);
	CHECK_SIZE("realloc", root, 60);
	CHECK_BLOCKS("realloc", p1, 4);

	p1 = talloc_realloc_size(NULL, p1, 20);
	CHECK_SIZE("realloc", p1, 60);

	talloc_increase_ref_count(p2);
	torture_assert("realloc", talloc_realloc_size(NULL, p2, 5) == NULL,
		"failed: talloc_realloc() on a referenced pointer should fail\n");
	CHECK_BLOCKS("realloc", p1, 4);

	ok1(talloc_realloc_size(NULL, p2, 0) == NULL);
	ok1(talloc_realloc_size(NULL, p2, 0) == NULL);
	CHECK_BLOCKS("realloc", p1, 3);

	torture_assert("realloc", talloc_realloc_size(NULL, p1, 0x7fffffff) == NULL,
		"failed: oversize talloc should fail\n");

	p1 = talloc_realloc_size(NULL, p1, 0);

	CHECK_BLOCKS("realloc", root, 1);
	CHECK_SIZE("realloc", root, 0);

	talloc_free(root);

	return true;
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

	root = talloc_new(ctx);

	el1 = talloc(root, struct el1);
	el1->list = talloc(el1, struct el2 *);
	el1->list[0] = talloc(el1->list, struct el2);
	el1->list[0]->name = talloc_strdup(el1->list[0], "testing");

	el1->list2 = talloc(el1, struct el2 *);
	el1->list2[0] = talloc(el1->list2, struct el2);
	el1->list2[0]->name = talloc_strdup(el1->list2[0], "testing2");

	el1->list3 = talloc(el1, struct el2 *);
	el1->list3[0] = talloc(el1->list3, struct el2);
	el1->list3[0]->name = talloc_strdup(el1->list3[0], "testing2");
	
	el2 = talloc(el1->list, struct el2);
	el2 = talloc(el1->list2, struct el2);
	el2 = talloc(el1->list3, struct el2);

	el1->list = talloc_realloc(el1, el1->list, struct el2 *, 100);
	el1->list2 = talloc_realloc(el1, el1->list2, struct el2 *, 200);
	el1->list3 = talloc_realloc(el1, el1->list3, struct el2 *, 300);

	talloc_free(root);

	return true;
}

/*
  test type checking
*/
static bool test_type(const struct torture_context *ctx)
{
	void *root;
	struct el1 {
		int count;
	};
	struct el2 {
		int count;
	};
	struct el1 *el1;

	root = talloc_new(ctx);

	el1 = talloc(root, struct el1);

	el1->count = 1;

	torture_assert("type", talloc_get_type(el1, struct el1) == el1,
		"type check failed on el1\n");
	torture_assert("type", talloc_get_type(el1, struct el2) == NULL,
		"type check failed on el1 with el2\n");
	talloc_set_type(el1, struct el2);
	torture_assert("type", talloc_get_type(el1, struct el2) == (struct el2 *)el1,
		"type set failed on el1 with el2\n");

	talloc_free(root);

	return true;
}

/*
  test steal
*/
static bool test_steal(const struct torture_context *ctx)
{
	void *root, *p1, *p2;

	root = talloc_new(ctx);

	p1 = talloc_array(root, char, 10);
	CHECK_SIZE("steal", p1, 10);

	p2 = talloc_realloc(root, NULL, char, 20);
	CHECK_SIZE("steal", p1, 10);
	CHECK_SIZE("steal", root, 30);

	torture_assert("steal", talloc_steal(p1, NULL) == NULL,
		"failed: stealing NULL should give NULL\n");

	torture_assert("steal", talloc_steal(p1, p1) == p1,
		"failed: stealing to ourselves is a nop\n");
	CHECK_BLOCKS("steal", root, 3);
	CHECK_SIZE("steal", root, 30);

	talloc_steal(NULL, p1);
	talloc_steal(NULL, p2);
	CHECK_BLOCKS("steal", root, 1);
	CHECK_SIZE("steal", root, 0);

	talloc_free(p1);
	talloc_steal(root, p2);
	CHECK_BLOCKS("steal", root, 2);
	CHECK_SIZE("steal", root, 20);
	
	talloc_free(p2);

	CHECK_BLOCKS("steal", root, 1);
	CHECK_SIZE("steal", root, 0);

	talloc_free(root);

	p1 = talloc_size(NULL, 3);
	CHECK_SIZE("steal", NULL, 3);
	talloc_free(p1);

	return true;
}

/*
  test move
*/
static bool test_move(const struct torture_context *ctx)
{
	void *root;
	struct t_move {
		char *p;
		int *x;
	} *t1, *t2;

	root = talloc_new(ctx);

	t1 = talloc(root, struct t_move);
	t2 = talloc(root, struct t_move);
	t1->p = talloc_strdup(t1, "foo");
	t1->x = talloc(t1, int);
	*t1->x = 42;

	t2->p = talloc_move(t2, &t1->p);
	t2->x = talloc_move(t2, &t1->x);
	torture_assert("move", t1->p == NULL && t1->x == NULL &&
	    strcmp(t2->p, "foo") == 0 && *t2->x == 42,
		"talloc move failed");

	talloc_free(root);

	return true;
}

/*
  test talloc_realloc_fn
*/
static bool test_realloc_fn(const struct torture_context *ctx)
{
	void *root, *p1;

	root = talloc_new(ctx);

	p1 = talloc_realloc_fn(root, NULL, 10);
	CHECK_BLOCKS("realloc_fn", root, 2);
	CHECK_SIZE("realloc_fn", root, 10);
	p1 = talloc_realloc_fn(root, p1, 20);
	CHECK_BLOCKS("realloc_fn", root, 2);
	CHECK_SIZE("realloc_fn", root, 20);
	p1 = talloc_realloc_fn(root, p1, 0);
	CHECK_BLOCKS("realloc_fn", root, 1);
	CHECK_SIZE("realloc_fn", root, 0);

	talloc_free(root);

	return true;
}


static bool test_unref_reparent(const struct torture_context *ctx)
{
	void *root, *p1, *p2, *c1;

	root = talloc_named_const(ctx, 0, "root");
	p1 = talloc_named_const(root, 1, "orig parent");
	p2 = talloc_named_const(root, 1, "parent by reference");

	c1 = talloc_named_const(p1, 1, "child");
	talloc_reference(p2, c1);

	CHECK_PARENT("unref_reparent", c1, p1);

	talloc_free(p1);

	CHECK_PARENT("unref_reparent", c1, p2);

	talloc_unlink(p2, c1);

	CHECK_SIZE("unref_reparent", root, 1);

	talloc_free(p2);
	talloc_free(root);

	return true;
}

static bool test_lifeless(const struct torture_context *ctx)
{
	void *top = talloc_new(ctx);
	char *parent, *child; 
	void *child_owner = talloc_new(ctx);

	parent = talloc_strdup(top, "parent");
	child = talloc_strdup(parent, "child");  
	(void)talloc_reference(child, parent);
	(void)talloc_reference(child_owner, child); 
	talloc_unlink(top, parent);
	talloc_free(child);
	talloc_free(top);
	talloc_free(child_owner);
	talloc_free(child);

	return true;
}

static int loop_destructor_count;

static int test_loop_destructor(char *ptr)
{
	loop_destructor_count++;
	return 0;
}

static bool test_loop(const struct torture_context *ctx)
{
	void *top = talloc_new(ctx);
	char *parent;
	struct req1 {
		char *req2, *req3;
	} *req1;

	parent = talloc_strdup(top, "parent");
	req1 = talloc(parent, struct req1);
	req1->req2 = talloc_strdup(req1, "req2");  
	talloc_set_destructor(req1->req2, test_loop_destructor);
	req1->req3 = talloc_strdup(req1, "req3");
	(void)talloc_reference(req1->req3, req1);
	talloc_free(parent);
	talloc_free(top);

	torture_assert("loop", loop_destructor_count == 1, 
				   "FAILED TO FIRE LOOP DESTRUCTOR\n");
	loop_destructor_count = 0;

	return true;
}

static int fail_destructor_str(char *ptr)
{
	return -1;
}

static bool test_free_parent_deny_child(const struct torture_context *ctx)
{
	void *top = talloc_new(ctx);
	char *level1;
	char *level2;
	char *level3;

	level1 = talloc_strdup(top, "level1");
	level2 = talloc_strdup(level1, "level2");
	level3 = talloc_strdup(level2, "level3");

	talloc_set_destructor(level3, fail_destructor_str);
	talloc_free(level1);
	talloc_set_destructor(level3, NULL);

	CHECK_PARENT("free_parent_deny_child", level3, top);

	talloc_free(top);

	return true;
}

static bool test_talloc_ptrtype(const struct torture_context *ctx)
{
	void *top = talloc_new(ctx);
	struct struct1 {
		int foo;
		int bar;
	} *s1, *s2, **s3, ***s4;
	const char *location1;
	const char *location2;
	const char *location3;
	const char *location4;

	s1 = talloc_ptrtype(top, s1);location1 = __location__;

	ok1(talloc_get_size(s1) == sizeof(struct struct1));

	ok1(strcmp(location1, talloc_get_name(s1)) == 0);

	s2 = talloc_array_ptrtype(top, s2, 10);location2 = __location__;

	ok1(talloc_get_size(s2) == (sizeof(struct struct1) * 10));

	ok1(strcmp(location2, talloc_get_name(s2)) == 0);

	s3 = talloc_array_ptrtype(top, s3, 10);location3 = __location__;

	ok1(talloc_get_size(s3) == (sizeof(struct struct1 *) * 10));

	torture_assert_str_equal("ptrtype", location3, talloc_get_name(s3),
		"talloc_array_ptrtype() sets the wrong name");

	s4 = talloc_array_ptrtype(top, s4, 10);location4 = __location__;

	ok1(talloc_get_size(s4) == (sizeof(struct struct1 **) * 10));

	torture_assert_str_equal("ptrtype", location4, talloc_get_name(s4),
		"talloc_array_ptrtype() sets the wrong name");

	talloc_free(top);

	return true;
}

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

	/* FIXME: Can't do nested destruction with locking, sorry. */
	if (ctx)
		return true;

	level0 = talloc_new(ctx);
	level1 = talloc_new(level0);
	level2 = talloc_new(level1);
	level3 = talloc_new(level2);
	level4 = talloc_new(level3);
	level5 = talloc(level4, void *);

	*level5 = talloc_reference(NULL, level3);

	test_talloc_free_in_destructor_run = false;
	talloc_set_destructor(level5, _test_talloc_free_in_destructor);

	talloc_free(level1);

	talloc_free(level0);

	ok1(test_talloc_free_in_destructor_run);

	return true;
}

static bool test_autofree(const struct torture_context *ctx)
{
	/* autofree test would kill smbtorture */
	void *p;
	p = talloc_autofree_context();
	talloc_free(p);

	p = talloc_autofree_context();
	talloc_free(p);

	return true;
}

static bool torture_local_talloc(const struct torture_context *tctx)
{
	bool ret = true;

	setlinebuf(stdout);

	talloc_disable_null_tracking();
	talloc_enable_null_tracking();

	ret &= test_ref1(tctx);
	ret &= test_ref2(tctx);
	ret &= test_ref3(tctx);
	ret &= test_ref4(tctx);
	ret &= test_unlink1(tctx); 
	ret &= test_misc(tctx);
	ret &= test_realloc(tctx);
	ret &= test_realloc_child(tctx); 
	ret &= test_steal(tctx);
	ret &= test_move(tctx);
	ret &= test_unref_reparent(tctx);
	ret &= test_realloc_fn(tctx);
	ret &= test_type(tctx);
	ret &= test_lifeless(tctx);
	ret &= test_loop(tctx);
	ret &= test_free_parent_deny_child(tctx);
	ret &= test_talloc_ptrtype(tctx);
	ret &= test_talloc_free_in_destructor(tctx);
	ret &= test_autofree(tctx);

	return ret;
}

static int lock_failed = 0, unlock_failed = 0, lock_bad = 0;
static int locked;

#define MAX_ALLOCATIONS 100
static void *allocations[MAX_ALLOCATIONS];
static int num_allocs, num_frees, num_reallocs;

static unsigned int find_ptr(const void *p)
{
	unsigned int i;

	for (i = 0; i < MAX_ALLOCATIONS; i++)
		if (allocations[i] == p)
			break;
	return i;
}

static unsigned int allocations_used(void)
{
	unsigned int i, ret = 0;

	for (i = 0; i < MAX_ALLOCATIONS; i++)
		if (allocations[i])
			ret++;
	return ret;
}

static void test_lock(const void *ctx)
{
	if (find_ptr(ctx) == MAX_ALLOCATIONS)
		lock_bad++;

	if (locked)
		lock_failed++;
	locked = 1;
}

static void test_unlock(void)
{
	if (!locked)
		unlock_failed++;
	locked = 0;
}

static int realloc_called, realloc_bad;

static void *normal_realloc(const void *parent, void *ptr, size_t size)
{
	unsigned int i = find_ptr(ptr);

	realloc_called++;
	if (ptr && size)
		num_reallocs++;
	else if (ptr)
		num_frees++;
	else if (size)
		num_allocs++;
	else
		abort();

	if (i == MAX_ALLOCATIONS) {
		if (ptr) {
			realloc_bad++;
			i = find_ptr(NULL);
		} else
			abort();
	}

	allocations[i] = realloc(ptr, size);
	/* Not guaranteed by realloc. */
	if (!size)
		allocations[i] = NULL;

	return allocations[i];
}

int main(void)
{
	struct torture_context *ctx;

	plan_tests(289);
	ctx = talloc_add_external(NULL, normal_realloc, test_lock, test_unlock);

	torture_local_talloc(NULL);
	ok(!lock_bad, "%u locks on bad pointer", lock_bad);
	ok(!lock_failed, "lock_failed count %u should be zero", lock_failed);
	ok(!unlock_failed, "unlock_failed count %u should be zero",
	   unlock_failed);
	ok(realloc_called == 1, "our realloc should not be called again");

	torture_local_talloc(ctx);
	ok(!lock_bad, "%u locks on bad pointer", lock_bad);
	ok(!lock_failed, "lock_failed count %u should be zero", lock_failed);
	ok(!unlock_failed, "unlock_failed count %u should be zero",
	   unlock_failed);
	ok(realloc_called, "our realloc should be called");
	ok(!realloc_bad, "our realloc given unknown pointer %u times",
	   realloc_bad);

	talloc_free(ctx);
	ok(!lock_bad, "%u locks on bad pointer", lock_bad);
	ok(!lock_failed, "lock_failed count %u should be zero", lock_failed);
	ok(!unlock_failed, "unlock_failed count %u should be zero",
	   unlock_failed);
	ok(realloc_called, "our realloc should be called");
	ok(!realloc_bad, "our realloc given unknown pointer %u times",
	   realloc_bad);

	ok(allocations_used() == 0, "%u allocations still used?",
	   allocations_used());

	/* This closes the leak, but make sure we're not freeing unexpected. */
	ok1(!talloc_chunk_from_ptr(null_context)->child);
	talloc_disable_null_tracking();
	
	return exit_status();
}

