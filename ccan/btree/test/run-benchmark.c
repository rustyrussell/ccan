/* Include the main header first, to test it works */
#include <ccan/btree/btree.h>
/* Include the C files directly. */
#include <ccan/btree/btree.c>
#include <ccan/tap/tap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t rand32_state = 0;

/*
 * Finds a pseudorandom 32-bit number from 0 to 2^32-1 .
 * Uses the BCPL linear congruential generator method.
 */
static uint32_t rand32(void)
{
	rand32_state *= (uint32_t)0x7FF8A3ED;
	rand32_state += (uint32_t)0x2AA01D31;
	return rand32_state;
}

static void scramble(void *base, size_t nmemb, size_t size)
{
	char *i = base;
	char *o;
	size_t sd;
	for (;nmemb>1;nmemb--) {
		o = i + size*(rand32()%nmemb);
		for (sd=size;sd--;) {
			char tmp = *o;
			*o++ = *i;
			*i++ = tmp;
		}
	}
}

struct test_item {
	size_t key;
	uint32_t value;
};

/* For ordering a btree of test_item pointers. */
static btree_search_implement (
	order_by_key,
	const struct test_item *,
	,
	a == b,
	a < b
)

/* For qsorting an array of test_item pointers. */
static int compare_test_item(const void *ap, const void *bp)
{
	const struct test_item *a = *(const struct test_item * const*)ap;
	const struct test_item *b = *(const struct test_item * const*)bp;
	if (a == b)
		return 0;
	if (a < b)
		return -1;
	return 1;
}

/*
 * If lr == 0, make sure iter points to the item given.
 * If lr == 1, make sure iter points to after the item given.
 */
static int check_iter(btree_iterator iter_orig, const void *item, int lr)
{
	btree_iterator iter = {*iter_orig};
	if (iter->item != item)
		return 0;
	if (lr) {
		if (!btree_prev(iter))
			return 0;
	} else {
		if (!btree_deref(iter))
			return 0;
	}
	if (iter->item != item)
		return 0;
	if (iter->node->item[iter->k] != iter->item)
		return 0;
	
	return 1;
}

/*
 * Returns 1 on insert, 0 on duplicate,
 * -1 on bad iterator returned by find, and
 * -2 on bad iterator returned by insert.
 */
static int insert_test_item(struct btree *btree, struct test_item *item)
{
	btree_iterator iter;
	int lr;
	
	/* Find the first or last matching item, randomly choosing between the two. */
	lr = rand32() & 1;
	if (btree_find_lr(btree, item, iter, lr)) {
		if (!check_iter(iter, item, lr))
			return -1;
		return 0;
	}
	
	btree_insert_at(iter, item);
	
	if (iter->item != item)
		return -2;
	
	return 1;
}

/*
 * Returns 1 on remove, 0 on missing,
 * -1 on bad iterator returned by find, and
 * -2 on bad iterator returned by remove.
 */
static int remove_test_item(struct btree *btree, struct test_item *item)
{
	btree_iterator iter;
	
	if (!btree_find(btree, item, iter))
		return 0;
	
	if (!check_iter(iter, item, 0))
		return -1;
	
	btree_remove_at(iter);
	
	if (iter->item != item)
		return -2;
	
	return 1;
}

static struct {
	size_t success;
	
	size_t excess;
	size_t duplicate;
	size_t missing;
	size_t incorrect;
	size_t failed;
	
	size_t bad_iter_find;
	size_t bad_iter_insert;
	size_t bad_iter_remove;
	size_t bad_iter_next;
} stats;

static void clear_stats(void) {
	memset(&stats, 0, sizeof(stats));
}

static int print_stats(const char *success_label, size_t expected_success) {
	int failed = 0;
	
	printf("      %s:  \t%zu\n", success_label, stats.success);
	if (stats.success != expected_success)
		failed = 1;
	
	if (stats.excess)
		failed = 1, printf("      Excess:     \t%zu\n", stats.excess);
	if (stats.duplicate)
		failed = 1, printf("      Duplicate:  \t%zu\n", stats.duplicate);
	if (stats.missing)
		failed = 1, printf("      Missing:    \t%zu\n", stats.missing);
	if (stats.incorrect)
		failed = 1, printf("      Incorrect:  \t%zu\n", stats.incorrect);
	if (stats.failed)
		failed = 1, printf("      Failed:     \t%zu\n", stats.failed);
	
	if (stats.bad_iter_find || stats.bad_iter_insert ||
	    stats.bad_iter_remove || stats.bad_iter_next) {
		failed = 1;
		printf("      Bad iterators yielded by:\n");
		if (stats.bad_iter_find)
			printf("          btree_find_lr(): %zu\n", stats.bad_iter_find);
		if (stats.bad_iter_insert)
			printf("          btree_insert_at(): %zu\n", stats.bad_iter_insert);
		if (stats.bad_iter_remove)
			printf("          btree_remove_at(): %zu\n", stats.bad_iter_remove);
		if (stats.bad_iter_next)
			printf("          btree_next(): %zu\n", stats.bad_iter_next);
	}
	
	return !failed;
}

static void benchmark(size_t max_per_trial, size_t round_count, int random_counts)
{
	struct test_item **test_item;
	struct test_item *test_item_array;
	size_t i, count;
	size_t round = 0;
	
	test_item = malloc(max_per_trial * sizeof(*test_item));
	test_item_array = malloc(max_per_trial * sizeof(*test_item_array));
	
	if (!test_item || !test_item_array) {
		fail("Not enough memory for %zu keys per trial\n",
			max_per_trial);
		return;
	}
	
	/* Initialize test_item pointers. */
	for (i=0; i<max_per_trial; i++)
		test_item[i] = &test_item_array[i];
	
	/*
	 * If round_count is not zero, run round_count trials.
	 * Otherwise, run forever.
	 */
	for (round = 1; round_count==0 || round <= round_count; round++) {
		struct btree *btree;
		btree_iterator iter;
		
		printf("Round %zu\n", round);
		
		if (random_counts)
			count = rand32() % (max_per_trial+1);
		else
			count = max_per_trial;
		
		/*
		 * Initialize test items by giving them sequential keys and
		 * random values. Scramble them so the order of insertion
		 * will be random.
		 */
		for (i=0; i<count; i++) {
			test_item[i]->key = i;
			test_item[i]->value = rand32();
		}
		scramble(test_item, count, sizeof(*test_item));
		
		btree = btree_new(order_by_key);
		
		clear_stats();
		printf("   Inserting %zu items...\n", count);
		for (i=0; i<count; i++) {
			switch (insert_test_item(btree, test_item[i])) {
				case 1: stats.success++; break;
				case 0: stats.duplicate++; break;
				case -1: stats.bad_iter_find++; break;
				case -2: stats.bad_iter_insert++; break;
				default: stats.failed++; break;
			}
		}
		ok1(print_stats("Inserted", count));
		
		scramble(test_item, count, sizeof(*test_item));
		
		printf("   Finding %zu items...\n", count);
		clear_stats();
		for (i=0; i<count; i++) {
			int lr = rand32() & 1;
			
			if (!btree_find_lr(btree, test_item[i], iter, lr)) {
				stats.missing++;
				continue;
			}
			
			if (!check_iter(iter, test_item[i], lr)) {
				stats.bad_iter_find++;
				continue;
			}
			
			stats.success++;
		}
		ok1(print_stats("Retrieved", count));
		
		qsort(test_item, count, sizeof(*test_item), compare_test_item);
		
		printf("   Traversing forward through %zu items...\n", count);
		clear_stats();
		i = 0;
		for (btree_begin(btree, iter); btree_next(iter);) {
			if (i >= count) {
				stats.excess++;
				continue;
			}
			
			if (iter->item == test_item[i])
				stats.success++;
			else
				stats.incorrect++;
			
			i++;
		}
		ok1(print_stats("Retrieved", count));
		
		printf("   Traversing backward through %zu items...\n", count);
		clear_stats();
		i = count;
		for (btree_end(btree, iter); btree_prev(iter);) {
			if (!i) {
				stats.excess++;
				continue;
			}
			i--;
			
			if (iter->item == test_item[i])
				stats.success++;
			else
				stats.incorrect++;
		}
		ok1(print_stats("Retrieved", count));
		
		ok1(btree->count == count);
		
		//static int remove_test_item(struct btree *btree, struct test_item *item);
		scramble(test_item, count, sizeof(*test_item));
		
		printf("   Deleting %zu items...\n", count);
		clear_stats();
		for (i=0; i<count; i++) {
			int s = remove_test_item(btree, test_item[i]);
			if (s != 1)
				printf("remove_test_item failed\n");
			switch (s) {
				case 1: stats.success++; break;
				case 0: stats.missing++; break;
				case -1: stats.bad_iter_find++; break;
				case -2: stats.bad_iter_remove++; break;
				default: stats.failed++; break;
			}
		}
		ok1(btree->count == 0);
		ok1(print_stats("Deleted", count));
		ok1(btree->root->depth == 0 && btree->root->count == 0);
		
		btree_delete(btree);
	}
	
	free(test_item);
	free(test_item_array);
}

int main(void)
{
	plan_tests(32);
	
	benchmark(300000, 4, 0);
	
	return exit_status();
}
