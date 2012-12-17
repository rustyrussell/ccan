#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

static const tal_t *locked;
static unsigned int lock_count, unlock_count, myalloc_count, myfree_count,
	myrealloc_count;

static void test_lockfn(const tal_t *ctx, void **pool_ptr)
{
	assert(!locked);
	locked = ctx;
	lock_count++;
}

static void test_unlockfn(const tal_t *ctx, void **pool_ptr)
{
	assert(locked == ctx);
	assert(unlock_count == lock_count-1);
	unlock_count++;
}

static void *my_alloc(size_t len, void **pool_ptr)
{
	void *ret;
	const size_t align = sizeof(void *)*2;
	size_t *hdr = *pool_ptr;

	assert(*hdr == 0);
	*hdr = len;

	ret = (char *)*pool_ptr + align;
	*pool_ptr = (char *)*pool_ptr + align + ((len + align-1) & ~(align-1));
	*(size_t *)*pool_ptr = 0;

	myalloc_count++;
	return ret;
}

static void my_free(void *p, void **pool_ptr)
{
	const size_t align = sizeof(void *)*2;
	size_t *hdr = (size_t *)((char *)p - align);

	assert(*hdr);
	*hdr = 0;
	myfree_count++;
}

static void *my_realloc(void *p, size_t len, void **pool_ptr)
{
	const size_t align = sizeof(void *)*2;
	size_t *hdr = (size_t *)((char *)p - align);
	void *new = my_alloc(len, pool_ptr);

	assert(len > *hdr);
	memcpy(new, p, *hdr);
	my_free(p, pool_ptr);

	myrealloc_count++;
	return new;
}

int main(void)
{
	char *parent, *c, *c2;
	void *pool, *pool_ptr;
	const void *start, *end;

	plan_tests(67);

	pool_ptr = pool = tal_arr(NULL, char, 32768);
	*(size_t *)pool_ptr = 0;
	start = bounds_start;
	end = bounds_end;

	/* Get first parent. */
	parent = tal_shared_init(sizeof(char), my_alloc, &pool_ptr);
	ok1(parent);
	ok1(myalloc_count == 1);
	ok1(lock_count == 0);
	ok1(unlock_count == 0);

	/* Doesn't update bounds! */
	ok1(bounds_start == start);
	ok1(bounds_end == end);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	tal_set_shared(pool, 32768, my_alloc, my_realloc, my_free,
		       test_lockfn, test_unlockfn, &pool_ptr);

	ok1(tal_check(parent, NULL));

	/* Allocate outside pool, is OK. */
	c = tal(NULL, char);
	ok1(myalloc_count == 0);
	ok1(lock_count == 0);
	ok1(unlock_count == 0);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	c = tal(parent, char);
	ok1(c);
	ok1(lock_count == 1);
	ok1(unlock_count == 1);
	ok1(locked == parent);
	/* Extra alloc for the parent's child property. */
	ok1(myalloc_count == 2);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	c2 = tal(c, char);
	ok1(c2);
	ok1(lock_count == 1);
	ok1(unlock_count == 1);
	ok1(locked == c);
	/* Extre alloc for c's child property */
	ok1(myalloc_count == 2);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	/* Free a single. */
	tal_free(c2);
	ok1(lock_count == 1);
	ok1(unlock_count == 1);
	ok1(locked == c);
	ok1(myalloc_count == 0);
	ok1(myfree_count == 1);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	/* Again, now free multiple. */
	c2 = tal(c, char);
	ok1(c2);
	ok1(lock_count == 1);
	ok1(unlock_count == 1);
	ok1(locked == c);
	ok1(myalloc_count == 1);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	/* First, do a check */
	ok1(tal_check(parent, NULL));

	tal_free(c);
	ok1(lock_count == 1);
	ok1(unlock_count == 1);
	ok1(locked == parent);
	/* Extra free, for child property! */
	ok1(myfree_count == 3);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	/* Now test realloc. */
	c = tal(parent, char);
	ok1(c);
	ok1(lock_count == 1);
	ok1(unlock_count == 1);
	ok1(locked == parent);
	ok1(myalloc_count == 1);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	c2 = c;
	ok1(tal_resize(&c, 128));
	ok1(c != c2);
	ok1(lock_count == 1);
	ok1(unlock_count == 1);
	ok1(locked == parent);
	ok1(myalloc_count == 1);
	ok1(myfree_count == 1);
	ok1(myrealloc_count == 1);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;
	tal_free(c);
	ok1(lock_count == 1);
	ok1(unlock_count == 1);
	ok1(locked == parent);
	ok1(myalloc_count == 0);
	ok1(myfree_count == 1);
	ok1(myrealloc_count == 0);
	locked = NULL;
	myalloc_count = myfree_count = myrealloc_count = 0;
	lock_count = unlock_count = 0;

	/* This shouldn't call our functions... */
	c2 = tal(NULL, char);
	ok1(tal_resize(&c2, 128));
	tal_free(c2);
	ok1(lock_count == 0);
	ok1(unlock_count == 0);
	ok1(locked == NULL);
	ok1(myalloc_count == 0);
	ok1(myfree_count == 0);
	ok1(myrealloc_count == 0);

	/* This should call free, but will not lock. */
	tal_free(parent);
	ok1(lock_count == 0);
	ok1(unlock_count == 0);
	ok1(locked == NULL);
	ok1(myalloc_count == 0);
	/* One extra for child property. */
	ok1(myfree_count == 2);
	ok1(myrealloc_count == 0);

	return exit_status();
}
