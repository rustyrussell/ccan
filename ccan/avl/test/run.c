/*
 * Copyright (C) 2010 Joseph Adams <joeyadams3.14159@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <ccan/avl/avl.h>

#define remove remove_
#include <ccan/avl/avl.c>
#undef remove

#include <ccan/tap/tap.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANIMATE_RANDOM_ACCESS 0

#if ANIMATE_RANDOM_ACCESS

#include <sys/time.h>

typedef int64_t msec_t;

static msec_t time_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (msec_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

#endif

uint32_t rand32_state = 0;

/*
 * Finds a pseudorandom 32-bit number from 0 to 2^32-1 .
 * Uses the BCPL linear congruential generator method.
 *
 * Note: this method (or maybe just this implementation) seems to
 *       go back and forth between odd and even exactly, which can
 *       affect low-cardinality testing if the cardinality given is even.
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

static struct {
	size_t success;
	size_t passed_invariants_checks;
	
	size_t excess;
	size_t duplicate;
	size_t missing;
	size_t incorrect;
	size_t failed;
	size_t failed_invariants_checks;
} stats;

static void clear_stats(void)
{
	memset(&stats, 0, sizeof(stats));
}

static bool print_stats(const char *success_label, size_t expected_success)
{
	int failed = 0;
	
	printf("      %s:  \t%zu\n", success_label, stats.success);
	if (stats.success != expected_success)
		failed = 1;
	
	if (stats.passed_invariants_checks)
		printf("      Passed invariants checks: %zu\n", stats.passed_invariants_checks);
	
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
	if (stats.failed_invariants_checks)
		failed = 1, printf("      Failed invariants checks: %zu\n", stats.failed_invariants_checks);
	
	return !failed;
}

struct test_item {
	uint32_t key;
	uint32_t value;
	size_t   insert_id; /* needed because qsort is not a stable sort */
};

static int compare_test_item(const void *ap, const void *bp)
{
	const struct test_item *a = *(void**)ap, *b = *(void**)bp;
	
	if (a->key < b->key)
		return -1;
	if (a->key > b->key)
		return 1;
	
	if (a->insert_id < b->insert_id)
		return -1;
	if (a->insert_id > b->insert_id)
		return 1;
	
	return 0;
}

static bool test_insert_item(AVL *avl, struct test_item *item)
{
	avl_insert(avl, &item->key, &item->value);
	return true;
}

static bool test_lookup_item(const AVL *avl, const struct test_item *item)
{
	return avl_member(avl, &item->key) && avl_lookup(avl, &item->key) == &item->value;
}

static bool test_remove_item(AVL *avl, struct test_item *item)
{
	return avl_remove(avl, &item->key);
}

static void test_foreach(AVL *avl, struct test_item **test_items, int count)
{
	AvlIter iter;
	int     i;
	
	i = 0;
	avl_foreach(iter, avl) {
		if (i >= count) {
			stats.excess++;
			continue;
		}
		if (iter.key == &test_items[i]->key && iter.value == &test_items[i]->value)
			stats.success++;
		else
			stats.incorrect++;
		i++;
	}
}

static void test_foreach_reverse(AVL *avl, struct test_item **test_items, int count)
{
	AvlIter iter;
	int     i;
	
	i = count - 1;
	avl_foreach_reverse(iter, avl) {
		if (i < 0) {
			stats.excess++;
			continue;
		}
		if (iter.key == &test_items[i]->key && iter.value == &test_items[i]->value)
			stats.success++;
		else
			stats.incorrect++;
		i--;
	}
}

static void benchmark(size_t max_per_trial, size_t round_count, bool random_counts, int cardinality)
{
	struct test_item **test_item;
	struct test_item *test_item_array;
	size_t i, o, count;
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
		AVL *avl;
		
		if (cardinality)
			printf("Round %zu (cardinality = %d)\n", round, cardinality);
		else
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
			test_item[i]->key   = rand32();
			test_item[i]->value = rand32();
			
			if (cardinality)
				test_item[i]->key %= cardinality;
		}
		scramble(test_item, count, sizeof(*test_item));
		
		avl = avl_new(order_u32_noctx);
		
		clear_stats();
		printf("   Inserting %zu items...\n", count);
		for (i=0; i<count; i++) {
			if (test_insert_item(avl, test_item[i])) {
				stats.success++;
				test_item[i]->insert_id = i;
			} else {
				printf("invariants check failed at insertion %zu\n", i);
				stats.failed++;
			}
			
			/* Periodically do an invariants check */
			if (count / 10 > 0 && i % (count / 10) == 0) {
				if (avl_check_invariants(avl))
					stats.passed_invariants_checks++;
				else
					stats.failed_invariants_checks++;
			}
		}
		ok1(print_stats("Inserted", count));
		ok1(avl_check_invariants(avl));
		
		/* remove early duplicates, as they are shadowed in insertions. */
		qsort(test_item, count, sizeof(*test_item), compare_test_item);
		for (i = 0, o = 0; i < count;) {
			uint32_t k = test_item[i]->key;
			do i++; while (i < count && test_item[i]->key == k);
			test_item[o++] = test_item[i-1];
		}
		count = o;
		ok1(avl_count(avl) == count);
		
		scramble(test_item, count, sizeof(*test_item));
		
		printf("   Finding %zu items...\n", count);
		clear_stats();
		for (i=0; i<count; i++) {
			if (!test_lookup_item(avl, test_item[i]))
				stats.missing++;
			else
				stats.success++;
		}
		ok1(print_stats("Retrieved", count));
		
		qsort(test_item, count, sizeof(*test_item), compare_test_item);
		
		printf("   Traversing forward through %zu items...\n", count);
		clear_stats();
		test_foreach(avl, test_item, count);
		ok1(print_stats("Traversed", count));
		
		printf("   Traversing backward through %zu items...\n", count);
		clear_stats();
		test_foreach_reverse(avl, test_item, count);
		ok1(print_stats("Traversed", count));
		
		scramble(test_item, count, sizeof(*test_item));
		
		printf("   Deleting %zu items...\n", count);
		clear_stats();
		for (i=0; i<count; i++) {
			if (test_remove_item(avl, test_item[i]))
				stats.success++;
			else
				stats.missing++;
			
			/* Periodically do an invariants check */
			if (count / 10 > 0 && i % (count / 10) == 0) {
				if (avl_check_invariants(avl))
					stats.passed_invariants_checks++;
				else
					stats.failed_invariants_checks++;
			}
		}
		ok1(print_stats("Deleted", count));
		ok1(avl_count(avl) == 0);
		ok1(avl_check_invariants(avl));
		
		avl_free(avl);
	}
	
	free(test_item);
	free(test_item_array);
}

static int compare_ptr(const void *a, const void *b)
{
	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

struct fail_total {
	int64_t fail;
	int64_t total;
};

static bool print_pass_fail(struct fail_total *pf, const char *label)
{
	long long fail  = pf->fail,
	          total = pf->total;
	
	printf("%s:\t%lld / %lld\n", label, total - fail, total);
	
	return fail == 0;
}

static void test_random_access(uint32_t max, int64_t ops)
{
	char       *in_tree;
	AVL        *avl;
	int64_t     i;
	struct {
		struct fail_total
			inserts, lookups, removes, checks;
	} s;
	
	#if ANIMATE_RANDOM_ACCESS
	msec_t last_update, now;
	msec_t interval = 100;
	#endif
	
	memset(&s, 0, sizeof(s));
	
	in_tree = malloc(max);
	memset(in_tree, 0, max);
	
	avl = avl_new(compare_ptr);
	
	#if ANIMATE_RANDOM_ACCESS
	now = time_ms();
	last_update = now - interval;
	#endif
	
	for (i = 0; i < ops; i++) {
		char *item = &in_tree[rand32() % max];
		char *found;
		bool  inserted, removed;
		
		#if ANIMATE_RANDOM_ACCESS
		now = time_ms();
		if (now >= last_update + interval) {
			last_update = now;
			printf("\r%.2f%%\t%zu / %zu\033[K", (double)i * 100 / ops, avl_count(avl), max);
			fflush(stdout);
		}
		#endif
		
		switch (rand32() % 3) {
			case 0:
				inserted = avl_insert(avl, item, item);
				
				if ((*item == 0 && !inserted) ||
				    (*item == 1 && inserted))
					s.inserts.fail++;
				
				if (inserted)
					*item = 1;
				
				s.inserts.total++;
				break;
			
			case 1:
				found = avl_lookup(avl, item);
				
				if ((*item == 0 && found != NULL) ||
				    (*item == 1 && found != item) ||
				    (avl_member(avl, item) != !!found))
					s.lookups.fail++;
				
				s.lookups.total++;
				break;
			
			case 2:
				removed = avl_remove(avl, item);
				
				if ((*item == 0 && removed) ||
				    (*item == 1 && !removed))
					s.removes.fail++;
				
				if (removed)
					*item = 0;
				
				s.removes.total++;
				break;
		}
		
		/* Periodically do an invariants check */
		if (ops / 10 > 0 && i % (ops / 10) == 0) {
			if (!avl_check_invariants(avl))
				s.checks.fail++;
			s.checks.total++;
		}
	}
	
	#if ANIMATE_RANDOM_ACCESS
	printf("\r\033[K");
	#endif
	
	avl_free(avl);
	free(in_tree);
	
	ok1(print_pass_fail(&s.inserts, "Inserts"));
	ok1(print_pass_fail(&s.lookups, "Lookups"));
	ok1(print_pass_fail(&s.removes, "Removes"));
	ok1(print_pass_fail(&s.checks,  "Invariants checks"));
}

int main(void)
{
	plan_tests(18 * 3 + 4);
	
	benchmark(100000, 2, false, 0);
	benchmark(100000, 2, false, 12345);
	benchmark(100000, 2, false, 100001);
	
	printf("Running random access test\n");
	test_random_access(12345, 1234567);
	
	return exit_status();
}
