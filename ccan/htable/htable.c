#include <ccan/htable/htable.h>
#include <ccan/compiler/compiler.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

/* This means a struct htable takes at least 512 bytes / 1k (32/64 bits). */
#define HTABLE_BASE_BITS 7

/* We use 0x1 as deleted marker. */
#define HTABLE_DELETED (0x1)

struct htable {
	size_t (*rehash)(const void *elem, void *priv);
	void *priv;
	unsigned int bits;
	size_t elems, max;
	/* These are the bits which are the same in all pointers. */
	uintptr_t common_mask, common_bits;
	uintptr_t *table;
};

/* We clear out the bits which are always the same, and put metadata there. */
static inline uintptr_t get_extra_ptr_bits(const struct htable *ht,
					   uintptr_t e)
{
	return e & ht->common_mask;
}

static inline void *get_raw_ptr(const struct htable *ht, uintptr_t e)
{
	return (void *)((e & ~ht->common_mask) | ht->common_bits);
}

static inline uintptr_t make_hval(const struct htable *ht,
				  const void *p, uintptr_t bits)
{
	return ((uintptr_t)p & ~ht->common_mask) | bits;
}

static inline bool entry_is_valid(uintptr_t e)
{
	return e > HTABLE_DELETED;
}

static inline uintptr_t get_hash_ptr_bits(const struct htable *ht,
					  size_t hash)
{
	/* Shuffling the extra bits (as specified in mask) down the
	 * end is quite expensive.  But the lower bits are redundant, so
	 * we fold the value first. */
	return (hash ^ (hash >> ht->bits)) & ht->common_mask;
}

struct htable *htable_new(size_t (*rehash)(const void *elem, void *priv),
			  void *priv)
{
	struct htable *ht = malloc(sizeof(struct htable));
	if (ht) {
		ht->bits = HTABLE_BASE_BITS;
		ht->rehash = rehash;
		ht->priv = priv;
		ht->elems = 0;
		ht->max = (1 << ht->bits) * 3 / 4;
		/* This guarantees we enter update_common first add. */
		ht->common_mask = -1;
		ht->common_bits = 0;
		ht->table = calloc(1 << ht->bits, sizeof(uintptr_t));
		if (!ht->table) {
			free(ht);
			ht = NULL;
		}
	}
	return ht;
}

void htable_free(const struct htable *ht)
{
	free((void *)ht->table);
	free((void *)ht);
}

static size_t hash_bucket(const struct htable *ht, size_t h)
{
	return h & ((1 << ht->bits)-1);
}

static void *htable_val(const struct htable *ht,
			struct htable_iter *i, size_t hash)
{
	uintptr_t h2 = get_hash_ptr_bits(ht, hash);

	while (ht->table[i->off]) {
		if (ht->table[i->off] != HTABLE_DELETED) {
			if (get_extra_ptr_bits(ht, ht->table[i->off]) == h2)
				return get_raw_ptr(ht, ht->table[i->off]);
		}
		i->off = (i->off + 1) & ((1 << ht->bits)-1);
	}
	return NULL;
}

void *htable_firstval(const struct htable *ht,
		      struct htable_iter *i, size_t hash)
{
	i->off = hash_bucket(ht, hash);
	return htable_val(ht, i, hash);
}

void *htable_nextval(const struct htable *ht,
		     struct htable_iter *i, size_t hash)
{
	i->off = (i->off + 1) & ((1 << ht->bits)-1);
	return htable_val(ht, i, hash);
}

void *htable_first(const struct htable *ht, struct htable_iter *i)
{
	for (i->off = 0; i->off < (size_t)1 << ht->bits; i->off++) {
		if (entry_is_valid(ht->table[i->off]))
			return get_raw_ptr(ht, ht->table[i->off]);
	}
	return NULL;
}

void *htable_next(const struct htable *ht, struct htable_iter *i)
{
	for (i->off++; i->off < (size_t)1 << ht->bits; i->off++) {
		if (entry_is_valid(ht->table[i->off]))
			return get_raw_ptr(ht, ht->table[i->off]);
	}
	return NULL;
}

/* This does not expand the hash table, that's up to caller. */
static void ht_add(struct htable *ht, const void *new, size_t h)
{
	size_t i;

	i = hash_bucket(ht, h);

	while (entry_is_valid(ht->table[i]))
		i = (i + 1) & ((1 << ht->bits)-1);

	ht->table[i] = make_hval(ht, new, get_hash_ptr_bits(ht, h));
}

static COLD_ATTRIBUTE bool double_table(struct htable *ht)
{
	unsigned int i;
	size_t oldnum = (size_t)1 << ht->bits;
	size_t *oldtable, e;

	oldtable = ht->table;
	ht->table = calloc(1 << (ht->bits+1), sizeof(size_t));
	if (!ht->table) {
		ht->table = oldtable;
		return false;
	}
	ht->bits++;
	ht->max *= 2;

	for (i = 0; i < oldnum; i++) {
		if (entry_is_valid(e = oldtable[i])) {
			void *p = get_raw_ptr(ht, e);
			ht_add(ht, p, ht->rehash(p, ht->priv));
		}
	}
	free(oldtable);
	return true;
}

/* We stole some bits, now we need to put them back... */
static COLD_ATTRIBUTE void update_common(struct htable *ht, const void *p)
{
	unsigned int i;
	uintptr_t maskdiff, bitsdiff;

	if (ht->elems == 0) {
		ht->common_mask = -1;
		ht->common_bits = (uintptr_t)p;
		return;
	}

	/* Find bits which are unequal to old common set. */
	maskdiff = ht->common_bits ^ ((uintptr_t)p & ht->common_mask);

	/* These are the bits which go there in existing entries. */
	bitsdiff = ht->common_bits & maskdiff;

	for (i = 0; i < (size_t)1 << ht->bits; i++) {
		if (!entry_is_valid(ht->table[i]))
			continue;
		/* Clear the bits no longer in the mask, set them as
		 * expected. */
		ht->table[i] &= ~maskdiff;
		ht->table[i] |= bitsdiff;
	}

	/* Take away those bits from our mask and set. */
	ht->common_mask &= ~maskdiff;
	ht->common_bits &= ~maskdiff;
}

bool htable_add(struct htable *ht, size_t hash, const void *p)
{
	if (ht->elems+1 > ht->max && !double_table(ht))
		return false;
	assert(p);
	if (((uintptr_t)p & ht->common_mask) != ht->common_bits)
		update_common(ht, p);

	ht_add(ht, p, hash);
	ht->elems++;
	return true;
}

/* If every one of the following buckets are DELETED (up to the next unused
   one), we can actually mark them all unused. */
static void delete_run(struct htable *ht, unsigned int num)
{
	unsigned int i, last = num + 1;
	size_t mask = (((size_t)1 << ht->bits)-1);

	while (ht->table[last & mask]) {
		if (entry_is_valid(ht->table[last & mask]))
			return;
		last++;
	}

	/* Now see if we can step backwards to find previous deleted ones. */
	for (i = num-1; ht->table[i & mask] == HTABLE_DELETED; i--);

	for (i++; i < last; i++)
		ht->table[i & ((1 << ht->bits)-1)] = 0;
}

bool htable_del(struct htable *ht, size_t h, const void *p)
{
	struct htable_iter i;
	void *c;

	for (c = htable_firstval(ht,&i,h); c; c = htable_nextval(ht,&i,h)) {
		if (c == p) {
			htable_delval(ht, &i);
			return true;
		}
	}
	return false;
}

void htable_delval(struct htable *ht, struct htable_iter *i)
{
	assert(i->off < (size_t)1 << ht->bits);
	assert(entry_is_valid(ht->table[i->off]));

	ht->elems--;
	ht->table[i->off] = HTABLE_DELETED;
	delete_run(ht, i->off);
}
