#include <ccan/hashtable/hashtable.h>
#include <ccan/hashtable/hashtable.c>
#include <ccan/tap/tap.h>
#include <stdbool.h>
#include <string.h>

#define NUM_VALS (1 << HASHTABLE_BASE_BITS)

/* We use the number divided by two as the hash (for lots of
   collisions), plus set all the higher bits so we can detect if they
   don't get masked out. */
static unsigned long hash(const void *elem, void *unused)
{
	unsigned long h = *(uint64_t *)elem / 2;
	h |= -1UL << HASHTABLE_BASE_BITS;
	return h;
}

static bool objcmp(const void *htelem, void *cmpdata)
{
	return *(uint64_t *)htelem == *(uint64_t *)cmpdata;
}

static void add_vals(struct hashtable *ht,
		     const uint64_t val[], unsigned int num)
{
	uint64_t i;

	for (i = 0; i < num; i++) {
		if (hashtable_find(ht, hash(&i, NULL), objcmp, &i)) {
			fail("%llu already in hash", (long long)i);
			return;
		}
		hashtable_add(ht, hash(&val[i], NULL), &val[i]);
		if (hashtable_find(ht, hash(&i, NULL), objcmp, &i) != &val[i]) {
			fail("%llu not added to hash", (long long)i);
			return;
		}
	}
	pass("Added %llu numbers to hash", (long long)i);
}

static void refill_vals(struct hashtable *ht,
			const uint64_t val[], unsigned int num)
{
	uint64_t i;

	for (i = 0; i < num; i++) {
		if (hashtable_find(ht, hash(&i, NULL), objcmp, &i))
			continue;
		hashtable_add(ht, hash(&val[i], NULL), &val[i]);
	}
}

static void find_vals(struct hashtable *ht,
		      const uint64_t val[], unsigned int num)
{
	uint64_t i;

	for (i = 0; i < num; i++) {
		if (hashtable_find(ht, hash(&i, NULL), objcmp, &i) != &val[i]) {
			fail("%llu not found in hash", (long long)i);
			return;
		}
	}
	pass("Found %llu numbers in hash", (long long)i);
}

static void del_vals(struct hashtable *ht,
		     const uint64_t val[], unsigned int num)
{
	uint64_t i;

	for (i = 0; i < num; i++) {
		if (!hashtable_del(ht, hash(&val[i], NULL), &val[i])) {
			fail("%llu not deleted from hash", (long long)i);
			return;
		}
	}
	pass("Deleted %llu numbers in hash", (long long)i);
}

struct travarg {
	unsigned int count;
	struct hashtable *ht;
	char touched[NUM_VALS];
	uint64_t *val;
};

static bool count(void *p, struct travarg *travarg)
{
	travarg->count++;
	travarg->touched[*(uint64_t *)p]++;
	return false;
}

static bool delete_self(uint64_t *p, struct travarg *travarg)
{
	travarg->count++;
	travarg->touched[*p]++;
	return !hashtable_del(travarg->ht, hash(p, NULL), p);
}

static bool delete_next(uint64_t *p, struct travarg *travarg)
{
	uint64_t *next = &travarg->val[((*p) + 1) % NUM_VALS];

	travarg->count++;
	travarg->touched[*p]++;
	return !hashtable_del(travarg->ht, hash(next, NULL), next);
}

static bool delete_prev(uint64_t *p, struct travarg *travarg)
{
	uint64_t *prev = &travarg->val[((*p) - 1) % NUM_VALS];

	travarg->count++;
	travarg->touched[*p]++;
	return !hashtable_del(travarg->ht, hash(prev, NULL), prev);
}

static bool stop_halfway(void *p, struct travarg *travarg)
{
	travarg->count++;
	travarg->touched[*(uint64_t *)p]++;

	return (travarg->count == NUM_VALS / 2);
}

static void check_all_touched_once(struct travarg *travarg)
{
	unsigned int i;

	for (i = 0; i < NUM_VALS; i++) {
		if (travarg->touched[i] != 1) {
			fail("Value %u touched %u times",
			     i, travarg->touched[i]);
			return;
		}
	}
	pass("All values touched once");
}

static void check_only_touched_once(struct travarg *travarg)
{
	unsigned int i;

	for (i = 0; i < NUM_VALS; i++) {
		if (travarg->touched[i] > 1) {
			fail("Value %u touched multiple (%u) times",
			     i, travarg->touched[i]);
			return;
		}
	}
	pass("No value touched twice");
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct hashtable *ht;
	uint64_t val[NUM_VALS];
	struct travarg travarg;
	uint64_t dne;

	plan_tests(20);
	for (i = 0; i < NUM_VALS; i++)
		val[i] = i;
	dne = i;

	ht = hashtable_new(hash, NULL);
	ok1(ht->max < (1 << ht->bits));
	ok1(ht->bits == HASHTABLE_BASE_BITS);

	/* We cannot find an entry which doesn't exist. */
	ok1(!hashtable_find(ht, hash(&dne, NULL), objcmp, &dne));

	/* Fill it, it should increase in size (once). */
	add_vals(ht, val, NUM_VALS);
	ok1(ht->bits == HASHTABLE_BASE_BITS + 1);
	ok1(ht->max < (1 << ht->bits));

	/* Find all. */
	find_vals(ht, val, NUM_VALS);
	ok1(!hashtable_find(ht, hash(&dne, NULL), objcmp, &dne));

	/* Delete all. */
	del_vals(ht, val, NUM_VALS);
	ok1(!hashtable_find(ht, hash(&val[0], NULL), objcmp, &val[0]));

	/* Refill. */
	refill_vals(ht, val, NUM_VALS);

	/* Traverse tests. */
	travarg.ht = ht;
	travarg.val = val;
	memset(travarg.touched, 0, sizeof(travarg.touched));
	travarg.count = 0;

	/* Traverse. */
	hashtable_traverse(ht, void, count, &travarg);
	ok1(travarg.count == NUM_VALS);
	check_all_touched_once(&travarg);

	memset(travarg.touched, 0, sizeof(travarg.touched));
	travarg.count = 0;
	hashtable_traverse(ht, void, stop_halfway, &travarg);
	ok1(travarg.count == NUM_VALS / 2);
	check_only_touched_once(&travarg);

	memset(travarg.touched, 0, sizeof(travarg.touched));
	travarg.count = 0;
	i = 0;
	/* Delete until we make no more progress. */
	for (;;) {
		hashtable_traverse(ht, uint64_t, delete_self, &travarg);
		if (travarg.count == i || travarg.count > NUM_VALS)
			break;
		i = travarg.count;
	}
	ok1(travarg.count == NUM_VALS);
	check_all_touched_once(&travarg);

	memset(travarg.touched, 0, sizeof(travarg.touched));
	travarg.count = 0;
	refill_vals(ht, val, NUM_VALS);
	hashtable_traverse(ht, uint64_t, delete_next, &travarg);
	ok1(travarg.count <= NUM_VALS);
	check_only_touched_once(&travarg);

	memset(travarg.touched, 0, sizeof(travarg.touched));
	travarg.count = 0;
	refill_vals(ht, val, NUM_VALS);
	hashtable_traverse(ht, uint64_t, delete_prev, &travarg);
	ok1(travarg.count <= NUM_VALS);
	check_only_touched_once(&travarg);

	return exit_status();
}
