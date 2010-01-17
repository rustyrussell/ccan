/* Include the main header first, to test it works */
#include <ccan/btree/btree.h>
/* Include the C files directly. */
#include <ccan/btree/btree.c>
#include <ccan/tap/tap.h>

#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*(array)))
#endif

struct foo {
	const char *string;
	int number;
};

struct foo foo_structs[] = {
	{"apple", 1},
	{"banana", 2},
	{"banana", 4},
	{"cherry", 4},
	{"doughnut", 5},
};

struct foo *foo_base[ARRAY_SIZE(foo_structs)];
const unsigned int foo_count = ARRAY_SIZE(foo_structs);

static void init_foo_pointers(void)
{
	unsigned int i;
	
	for (i = 0; i < foo_count; i++)
		foo_base[i] = &foo_structs[i];
}

/* Make sure forward declarations work */
btree_search_proto order_by_string, order_by_number;

static void test_order_by_string(void)
{
	struct {
		const char *key;
		int lr;
		unsigned int expect_offset;
		int expect_found;
	} test[] = {
		{"anchovies", 0, 0, 0},
		{"anchovies", 1, 0, 0},
		{"apple", 0, 0, 1},
		{"apple", 1, 1, 1},
		{"banana", 0, 1, 1},
		{"banana", 1, 3, 1},
		{"blueberry", 0, 3, 0},
		{"blueberry", 1, 3, 0},
		{"doughnut", 0, 4, 1},
		{"doughnut", 1, 5, 1},
	};
	
	size_t i;
	for (i=0; i<ARRAY_SIZE(test); i++) {
		struct foo foo = {test[i].key, 0};
		unsigned int offset;
		int found = 0;
		
		offset = order_by_string(&foo, (void*)foo_base, foo_count,
						test[i].lr, &found);
		
		ok1(offset == test[i].expect_offset && found == test[i].expect_found);
	}
}

static void test_empty(void)
{
	unsigned int offset;
	int found;
	struct foo key = {"apple", -1};
	
	offset = order_by_string(&key, NULL, 0, 0, &found);
	ok1(offset == 0);
	
	offset = order_by_string(&key, NULL, 0, 1, &found);
	ok1(offset == 0);
	
	offset = order_by_number(&key, NULL, 0, 0, &found);
	ok1(offset == 0);
	
	offset = order_by_number(&key, NULL, 0, 1, &found);
	ok1(offset == 0);
}

static void test_order_by_number(void)
{
	struct {
		int key;
		int lr;
		unsigned int expect_offset;
		int expect_found;
	} test[] = {
		{-2, 0, 0, 0},
		{-2, 1, 0, 0},
		{-1, 0, 0, 0},
		{-1, 1, 0, 0},
		{0, 0, 0, 0},
		{0, 1, 0, 0},
		{1, 0, 0, 1},
		{1, 1, 1, 1},
		{2, 0, 1, 1},
		{2, 1, 2, 1},
		{4, 0, 2, 1},
		{4, 1, 4, 1},
		{3, 0, 2, 0},
		{3, 1, 2, 0},
		{5, 0, 4, 1},
		{5, 1, 5, 1},
		{6, 0, 5, 0},
		{6, 1, 5, 0},
		{7, 0, 5, 0},
		{7, 1, 5, 0},
	};
	
	size_t i;
	for (i=0; i<ARRAY_SIZE(test); i++) {
		struct foo foo = {"", test[i].key};
		unsigned int offset;
		int found = 0;
		
		offset = order_by_number(&foo, (void*)foo_base, foo_count,
						test[i].lr, &found);
		
		ok1(offset == test[i].expect_offset && found == test[i].expect_found);
	}
}

int main(void)
{
	plan_tests(34);
	init_foo_pointers();
	
	test_order_by_string();
	test_order_by_number();
	test_empty();
	
	return exit_status();
}

btree_search_implement (
	order_by_string,
	const struct foo *,
	int c = strcmp(a->string, b->string),
	c == 0,
	c < 0
)

btree_search_implement (
	order_by_number,
	const struct foo *,
	,
	a->number == b->number,
	a->number < b->number
)
