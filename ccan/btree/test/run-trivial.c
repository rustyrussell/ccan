/* Include the main header first, to test it works */
#include <ccan/btree/btree.h>
/* Include the C files directly. */
#include <ccan/btree/btree.c>
#include <ccan/tap/tap.h>

#include <string.h>

struct test_item {
	int key;
	int value;
};

static btree_search_implement(
	order_by_key,
	struct test_item *,
	,
	a->key == b->key,
	a->key < b->key
)

static int insert_test_item(struct btree *btree, int key, int value)
{
	struct test_item key_item = {key, -101};
	struct test_item *item;
	btree_iterator iter;
	
	if (btree_find_first(btree, &key_item, iter)) {
		/* Don't insert new item, but do update its value. */
		item = iter->item;
		item->value = value;
		return 0;
	}
	
	item = malloc(sizeof(*item));
	item->key = key;
	item->value = value;
	
	btree_insert_at(iter, item);
	
	return 1;
}

static int lookup_test_item(const struct btree *btree, int key)
{
	struct test_item key_item = {key, -102};
	struct test_item *item;
	btree_iterator iter;
	
	if (!btree_find_first(btree, &key_item, iter))
		return -100;
	
	item = iter->item;
	return item->value;
}

static int destroy_test_item(void *item, void *ctx) {
	(void) ctx;
	free(item);
	return 1;
}

struct test_insert_entry {
	int key;
	int value;
	int expected_return;
};

struct test_traverse_entry {
	int key;
	int value;
};

static void print_indent(unsigned int indent) {
	while (indent--)
		fputs("\t", stdout);
}

static void btree_node_trace(struct btree_node *node, unsigned int indent)
{
	unsigned int i;
	for (i=0; i<node->count; i++) {
		if (node->depth)
			btree_node_trace(node->branch[i], indent+1);
		print_indent(indent);
		puts(node->item[i]);
	}
	if (node->depth)
		btree_node_trace(node->branch[node->count], indent+1);
}

static void btree_trace(struct btree *btree)
{
	btree_node_trace(btree->root, 0);
}

static void test_insert(struct btree *btree)
{
	struct test_insert_entry ent[] = {
		{3, 1, 1}, {4, 1, 1}, {5, 9, 1}, {2, 6, 1}, {5, 3, 0}, {5, 8, 0},
		{9, 7, 1}, {9, 3, 0}, {2, 3, 0}, {8, 4, 1}, {6, 2, 1}, {6, 4, 0},
		{3, 3, 0}, {8, 3, 0}, {2, 7, 0}, {9, 5, 0}, {0, 2, 1}, {8, 8, 0},
		{4, 1, 0}, {9, 7, 0}, {1, 6, 1}, {9, 3, 0}, {9, 9, 0}, {3, 7, 0},
		{5, 1, 0}, {0, 5, 0}, {8, 2, 0}, {0, 9, 0}, {7, 4, 1}, {9, 4, 0},
		{4, 5, 0}, {9, 2, 0}
	};
	size_t i, count = sizeof(ent) / sizeof(*ent);
	
	for (i = 0; i < count; i++) {
		int ret = insert_test_item(btree, ent[i].key, ent[i].value);
		ok1(ret == ent[i].expected_return);
	}
}

static void test_find_traverse(struct btree *btree)
{
	struct test_traverse_entry ent[] = {
		{0, 9}, {1, 6}, {2, 7}, {3, 7}, {4, 5},
		{5, 1}, {6, 4}, {7, 4}, {8, 2}, {9, 2}
	};
	size_t i, count = sizeof(ent) / sizeof(*ent);
	btree_iterator iter;
	
	i = 0;
	for (btree_begin(btree, iter); btree_next(iter);) {
		struct test_item *item = iter->item;
		
		if (i >= count) {
			fail("Too many items in btree according to forward traversal");
			break;
		}
		
		ok1(lookup_test_item(btree, item->key) == item->value);
		ok1(item->key == ent[i].key && item->value == ent[i].value);
		
		i++;
	}
	
	if (i != count)
		fail("Not enough items in btree according to forward traversal");
	
	i = count;
	for (btree_end(btree, iter); btree_prev(iter);) {
		struct test_item *item = iter->item;
		
		if (!i--) {
			fail("Too many items in btree according to backward traversal");
			break;
		}
		
		ok1(lookup_test_item(btree, item->key) == item->value);
		ok1(item->key == ent[i].key && item->value == ent[i].value);
	}
	
	if (i != 0)
		fail("Not enough items in btree according to backward traversal");
}

static btree_search_proto order_by_string;

static btree_search_implement(
	order_by_string, //function name
	const char*, //key type
	int c = strcmp(a, b), //setup
	c == 0, // a == b predicate
	c < 0 // a < b predicate
)

//used in the test case to sort the test strings
static int compare_by_string(const void *ap, const void *bp)
{
	const char * const *a = ap;
	const char * const *b = bp;
	return strcmp(*a, *b);
}

static void test_traverse(struct btree *btree, const char *sorted[], size_t count)
{
	btree_iterator iter, iter2;
	size_t i;
	
	i = 0;
	for (btree_begin(btree, iter); btree_next(iter);) {
		if (i >= count) {
			fail("Too many items in btree according to forward traversal");
			break;
		}
		
		ok1(iter->item == sorted[i]);
		
		btree_find_first(btree, sorted[i], iter2);
		ok1(iter2->item == sorted[i]);
		
		i++;
	}
	
	if (i != count)
		fail("Not enough items in btree according to forward traversal");
	
	i = count;
	for (btree_end(btree, iter); btree_prev(iter);) {
		if (!i--) {
			fail("Too many items in btree according to backward traversal");
			break;
		}
		
		ok1(iter->item == sorted[i]);
		
		btree_find_first(btree, sorted[i], iter2);
		ok1(iter2->item == sorted[i]);
	}
	
	if (i != 0)
		fail("Not enough items in btree according to backward traversal");
}

#if 0
//(tries to) remove the key from both the btree and the test array.  Returns 1 on success, 0 on failure.
//success is when an item is removed from the btree and the array, or when none is removed from either
//failure is when an item is removed from the btree but not the array or vice versa
//after removing, it tries removing again to make sure that removal returns 0
static int test_remove(struct btree *btree, struct btree_item items[], size_t *count, const char *key)
{
	size_t i;
	
	for (i = *count; i--;) {
		if (!strcmp(items[i].key, key)) {
			//item found in array
			memmove(&items[i], &items[i+1], (*count-(i+1))*sizeof(*items));
			(*count)--;
			
			//puts("----------");
			//btree_trace(btree);
			
			//removal should succeed, as the key should be there
			//this is not a contradiction; the test is performed twice
			return btree_remove(btree, key) && !btree_remove(btree, key);
		}
	}
	
	//removal should fail, as the key should not be there
	//this is not redundant; the test is performed twice
	return !btree_remove(btree, key) && !btree_remove(btree, key);
}
#endif

static void test_search_implement(void)
{
	struct btree *btree = btree_new(order_by_string);
	size_t i;
	
	const char *unsorted[] = {
		"md4",
		"isaac",
		"noerr",
		"talloc_link",
		"asearch",
		"tap",
		"crcsync",
		"wwviaudio",
		"array_size",
		"alignof",
		"str",
		"read_write_all",
		"grab_file",
		"out",
		"daemonize",
		"array",
		"crc",
		"str_talloc",
		"build_assert",
		"talloc",
		"alloc",
		"endian",
		"btree",
		"typesafe_cb",
		"check_type",
		"list",
		"ciniparser",
		"ilog",
		"ccan_tokenizer",
		"tdb",
		"block_pool",
		"sparse_bsearch",
		"container_of",
		"stringmap",
		"hash",
		"short_types",
		"ogg_to_pcm",
		"antithread",
	};
	size_t count = sizeof(unsorted) / sizeof(*unsorted);
	const char *sorted[count];
	
	memcpy(sorted, unsorted, sizeof(sorted));
	qsort(sorted, count, sizeof(*sorted), compare_by_string);
	
	for (i=0; i<count; i++) {
		btree_iterator iter;
		
		if (btree_find_first(btree, unsorted[i], iter))
			fail("btree_insert thinks the test array has duplicates, but it doesn't");
		else
			btree_insert_at(iter, unsorted[i]);
	}
	btree_trace(btree);
	
	test_traverse(btree, sorted, count);
	
	/*
	//try removing items that should be in the tree
	ok1(test_remove(btree, sorted, &count, "btree"));
	ok1(test_remove(btree, sorted, &count, "ccan_tokenizer"));
	ok1(test_remove(btree, sorted, &count, "endian"));
	ok1(test_remove(btree, sorted, &count, "md4"));
	ok1(test_remove(btree, sorted, &count, "asearch"));
	ok1(test_remove(btree, sorted, &count, "alloc"));
	ok1(test_remove(btree, sorted, &count, "build_assert"));
	ok1(test_remove(btree, sorted, &count, "typesafe_cb"));
	ok1(test_remove(btree, sorted, &count, "sparse_bsearch"));
	ok1(test_remove(btree, sorted, &count, "stringmap"));
	
	//try removing items that should not be in the tree
	ok1(test_remove(btree, sorted, &count, "java"));
	ok1(test_remove(btree, sorted, &count, "openoffice"));
	ok1(test_remove(btree, sorted, &count, "firefox"));
	ok1(test_remove(btree, sorted, &count, "linux"));
	ok1(test_remove(btree, sorted, &count, ""));
	
	//test the traversal again to make sure things are okay
	test_traverse(btree, sorted, count);
	
	//remove the rest of the items, then make sure tree is empty
	while (count)
		ok1(test_remove(btree, sorted, &count, sorted[count-1].key));
	ok1(btree->root == NULL);
	*/
	
	btree_delete(btree);
}

int main(void)
{
	struct btree *btree;
	
	plan_tests(224);
	
	btree = btree_new(order_by_key);
	btree->destroy = destroy_test_item;
	test_insert(btree);
	test_find_traverse(btree);
	btree_delete(btree);
	
	test_search_implement();
	
	return exit_status();
}
