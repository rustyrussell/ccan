/* Include the main header first, to test it works */
#include <ccan/btree/btree.h>
/* Include the C files directly. */
#include <ccan/btree/btree.c>
#include <ccan/tap/tap.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rand32_state = 0;

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

/*
 * Whether or not to add/remove multiple equal keys to the tree.
 *
 * Tests are run with multi set to 0 and 1.
 */
static int multi = 0;

static struct {
	struct {
		size_t success;
		size_t failure;
	} insert, remove, find, traverse;
} stats;

static int check_stats(void) {
	return
		stats.insert.failure == 0 &&
		stats.remove.failure == 0 &&
		stats.find.failure == 0 &&
		stats.traverse.failure == 0;
}

static int print_stats(void) {
	printf("Insert:  %zu succeeded, %zu failed\n",
		stats.insert.success, stats.insert.failure);
	
	printf("Remove:  %zu succeeded, %zu failed\n",
		stats.remove.success, stats.remove.failure);
	
	printf("Find:  %zu succeeded, %zu failed\n",
		stats.find.success, stats.find.failure);
	
	printf("Traverse:  %zu succeeded, %zu failed\n",
		stats.traverse.success, stats.traverse.failure);
	
	return check_stats();
}

static void clear_stats(void)
{
	memset(&stats, 0, sizeof(stats));
}

static int test_node_consistency(struct btree_node *node, struct btree_node *parent, size_t *count)
{
	unsigned int i, j, e = node->count;
	
	/* Verify parent, depth, and k */
	if (node->parent != parent)
		return 0;
	if (parent) {
		if (node->depth != parent->depth - 1)
			return 0;
		if (node != parent->branch[node->k])
			return 0;
	}
	
	/* Nodes must not be empty (unless the entire tree is empty). */
	if (e == 0)
		return 0;
	
	if (node->depth) {
		/* Make sure child branches aren't duplicated or NULL. */
		for (i=0; i<=e; i++) {
			if (node->branch[i] == NULL)
				return 0;
			for (j=i+1; j<=e; j++)
				if (node->branch[i] == node->branch[j])
					return 0;
		}
		
		/* Recursively check children. */
		for (i=0; i<=e; i++) {
			if (!test_node_consistency(node->branch[i], node, count))
				return 0;
		}
	}
	
	*count += node->count;
	return 1;
}

static int test_consistency(const struct btree *btree)
{
	size_t count = 0;
	if (!btree->root)
		return 0;
	if (btree->root->count == 0) {
		if (btree->count != 0)
			return 0;
		return 1;
	}
	if (btree->count == 0)
		return 0;
	if (!test_node_consistency(btree->root, NULL, &count))
		return 0;
	if (btree->count != count)
		return 0;
	return 1;
}

static int test_traverse(struct btree *btree, size_t key[], size_t count)
{
	btree_iterator iter;
	size_t i, j;
	
	if (!test_consistency(btree))
		return 0;
	
	/* Forward */
	i = 0;
	btree_begin(btree, iter);
	for (;;) {
		while (i < count && key[i] == 0)
			i++;
		if (i >= count) {
			if (btree_next(iter))
				return 0;
			break;
		}
		for (j = 0; j < key[i] && btree_next(iter); j++) {
			if (iter->item != &key[i])
				return 0;
		}
		if (j != key[i])
			return 0;
		i++;
	}
	
	/* Backward */
	i = count;
	btree_end(btree, iter);
	for (;;) {
		while (i > 0 && key[i-1] == 0)
			i--;
		if (i <= 0) {
			if (btree_prev(iter))
				return 0;
			break;
		}
		for (j = 0; j < key[i-1] && btree_prev(iter); j++) {
			if (iter->item != &key[i-1])
				return 0;
		}
		if (j != key[i-1])
			return 0;
		i--;
	}
	
	return 1;
}

/*
 * Finds and counts items matching &key[k], then makes sure the count
 * equals key[k].  Also verifies that btree_find_... work as advertised.
 */
static int find(struct btree *btree, size_t key[], size_t k)
{
	btree_iterator iter, tmp;
	size_t count;
	int found;
	
	memset(iter, 0, sizeof(iter));
	memset(tmp, 0, sizeof(tmp));
	
	found = btree_find_first(btree, &key[k], iter);
	if (iter->btree != btree)
		return 0;
	if (found != !!key[k])
		return 0;
	if (key[k] && iter->item != &key[k])
		return 0;
	
	/* Make sure btree_find works exactly the same as btree_find_first. */
	if (btree_find(btree, &key[k], tmp) != found)
		return 0;
	if (memcmp(iter, tmp, sizeof(*iter)))
		return 0;
	
	/* Make sure previous item doesn't match. */
	*tmp = *iter;
	if (btree_prev(tmp)) {
		if (tmp->item == &key[k])
			return 0;
	}
	
	/* Count going forward. */
	for (count=0; btree_deref(iter) && iter->item == &key[k]; count++, btree_next(iter))
		{}
	if (count != key[k])
		return 0;
	
	/* Make sure next item doesn't match. */
	*tmp = *iter;
	if (btree_deref(tmp)) {
		if (tmp->item == &key[k])
			return 0;
	}
	
	/* Make sure iter is now equal to the end of matching items. */
	btree_find_last(btree, &key[k], tmp);
	if (tmp->btree != btree)
		return 0;
	if (btree_cmp_iters(iter, tmp))
		return 0;
	
	/* Count going backward. */
	for (count=0; btree_prev(iter); count++) {
		if (iter->item != &key[k]) {
			btree_next(iter);
			break;
		}
	}
	if (count != key[k])
		return 0;
	
	/* Make sure iter is now equal to the beginning of matching items. */
	btree_find_first(btree, &key[k], tmp);
	if (tmp->btree != btree)
		return 0;
	if (btree_cmp_iters(iter, tmp))
		return 0;
	
	return 1;
}

static int test_find(struct btree *btree, size_t key[], size_t count)
{
	size_t k = rand32() % count;
	return find(btree, key, k);
}

static int test_remove(struct btree *btree, size_t key[], size_t count)
{
	size_t prev_count = btree->count;
	size_t k = rand32() % count;
	btree_iterator iter;
	
	if (!find(btree, key, k))
		return 0;
	
	btree_find(btree, &key[k], iter);
	
	//remove (if present), and make sure removal status is correct
	if (key[k]) {
		if (btree_remove_at(iter) != 1)
			return 0;
		if (btree->count != prev_count - 1)
			return 0;
		key[k]--;
		
		if (!find(btree, key, k))
			return 0;
	}
	
	return 1;
}

static int test_insert(struct btree *btree, size_t key[], size_t count)
{
	size_t k = rand32() % count;
	btree_iterator iter;
	size_t prev_count = btree->count;
	int found;
	
	if (!find(btree, key, k))
		return 0;
	
	/* Make sure key's presence is consistent with our array. */
	found = btree_find_first(btree, &key[k], iter);
	if (key[k]) {
		if (!found || iter->item != &key[k])
			return 0;
		if (!btree_deref(iter))
			return 0;
		if (iter->k >= iter->node->count || iter->node->item[iter->k] != &key[k])
			return 0;
	} else {
		if (found)
			return 0;
	}
	
	/* Insert if item hasn't been yet (or if we're in multi mode). */
	if (!key[k] || multi) {
		btree_insert_at(iter, &key[k]);
		key[k]++;
		if (btree->count != prev_count + 1)
			return 0;
	}
	
	/* Check result iterator's ->item field. */
	if (iter->item != &key[k])
		return 0;
	
	if (!find(btree, key, k))
		return 0;
	
	/* Make sure key is present and correct now. */
	found = btree_find_first(btree, &key[k], iter);
	if (!found || iter->item != &key[k])
		return 0;
	
	return 1;
}

static btree_search_implement(order_by_ptr, size_t*, , a == b, a < b)

static void stress(size_t count, size_t trials)
{
	struct btree *btree = btree_new(order_by_ptr);
	size_t *key = calloc(count, sizeof(*key));
	size_t i;
	
	clear_stats();
	
	for (i=0; i<trials; i++) {
		unsigned int n = rand32() % 65536;
		unsigned int rib = btree->count * 43688 / count;
			//remove/insert boundary
		if (n >= 65534) {
			if (test_traverse(btree, key, count))
				stats.traverse.success++;
			else
				stats.traverse.failure++;
		} else if (n >= 46388) {
			if (test_find(btree, key, count))
				stats.find.success++;
			else
				stats.find.failure++;
		} else if (n < rib) {
			if (test_remove(btree, key, count))
				stats.remove.success++;
			else
				stats.remove.failure++;
		} else {
			if (test_insert(btree, key, count))
				stats.insert.success++;
			else
				stats.insert.failure++;
		}
	}
	
	free(key);
	btree_delete(btree);
	
	print_stats();
	ok1(check_stats());
}

int main(void)
{
	plan_tests(2);
	
	multi = 0;
	printf("Running with multi = %d\n", multi);
	stress(100000, 500000);
	
	multi = 1;
	printf("Running with multi = %d\n", multi);
	stress(100000, 500000);
	
	return exit_status();
}
