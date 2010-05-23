#include <ccan/hashtable/hashtable.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

/* This means a struct hashtable takes at least 512 bytes / 1k (32/64 bits). */
#define HASHTABLE_BASE_BITS 7

/* We use 0x1 as deleted marker. */
#define HASHTABLE_DELETED ((void *)0x1)

/* Number of redundant bits in a pointer. */
#define PTR_STEAL_BITS 3

static inline uintptr_t get_extra_ptr_bits(const void *p)
{
	return (uintptr_t)p & (((uintptr_t)1 << PTR_STEAL_BITS)-1);
}

static inline void *get_raw_ptr(const void *p)
{
	return (void *)((uintptr_t)p & ~(((uintptr_t)1 << PTR_STEAL_BITS)-1));
}

static inline void *make_ptr(const void *p, uintptr_t bits)
{
	return (void *)((uintptr_t)p | bits);
}

struct hashtable {
	unsigned long (*rehash)(const void *elem, void *priv);
	void *priv;
	unsigned int bits;
	unsigned long elems, max;
	void **table;
};

static inline bool ptr_is_valid(const void *e)
{
	return e > HASHTABLE_DELETED;
}

struct hashtable *hashtable_new(unsigned long (*rehash)(const void *elem,
							void *priv),
				void *priv)
{
	struct hashtable *ht = malloc(sizeof(struct hashtable));
	if (ht) {
		ht->bits = HASHTABLE_BASE_BITS;
		ht->rehash = rehash;
		ht->priv = priv;
		ht->elems = 0;
		ht->max = (1 << ht->bits) * 3 / 4;
		ht->table = calloc(1 << ht->bits, sizeof(void *));
		if (!ht->table) {
			free(ht);
			ht = NULL;
		}
	}
	return ht;
}

void hashtable_free(struct hashtable *ht)
{
	free(ht->table);
	free(ht);
}

static unsigned long hash_bucket(const struct hashtable *ht,
				 unsigned long h, unsigned long *ptrbits)
{
	*ptrbits = (h >> ht->bits) & (((uintptr_t)1 << PTR_STEAL_BITS)-1);
	return h & ((1 << ht->bits)-1);
}

/* This does not expand the hash table, that's up to caller. */
static void ht_add(struct hashtable *ht, const void *new, unsigned long h)
{
	unsigned long i, h2;

	i = hash_bucket(ht, h, &h2);

	while (ptr_is_valid(ht->table[i])) {
		i = (i + 1) & ((1 << ht->bits)-1);
	}
	ht->table[i] = make_ptr(new, h2);
}

static bool double_table(struct hashtable *ht)
{
	unsigned int i;
	size_t oldnum = (size_t)1 << ht->bits;
	void **oldtable, *e;

	oldtable = ht->table;
	ht->table = calloc(1 << (ht->bits+1), sizeof(void *));
	if (!ht->table) {
		ht->table = oldtable;
		return false;
	}
	ht->bits++;
	ht->max *= 2;

	for (i = 0; i < oldnum; i++) {
		if (ptr_is_valid(e = oldtable[i])) {
			e = get_raw_ptr(e);
			ht_add(ht, e, ht->rehash(e, ht->priv));
		}
	}
	free(oldtable);
	return true;
}

bool hashtable_add(struct hashtable *ht, unsigned long hash, const void *p)
{
	if (ht->elems+1 > ht->max && !double_table(ht))
		return false;
	ht->elems++;
	assert(p);
	assert(((uintptr_t)p & (((uintptr_t)1 << PTR_STEAL_BITS)-1)) == 0);
	ht_add(ht, p, hash);
	return true;
}

/* Find bucket for an element. */
static size_t ht_find(struct hashtable *ht,
		      unsigned long hash,
		      bool (*cmp)(const void *htelem, void *cmpdata),
		      void *cmpdata)
{
	void *e;
	unsigned long i, h2;

	i = hash_bucket(ht, hash, &h2);

	while ((e = ht->table[i]) != NULL) {
		if (e != HASHTABLE_DELETED
		    && h2 == get_extra_ptr_bits(e)
		    && cmp(get_raw_ptr(ht->table[i]), cmpdata))
			return i;
		i = (i + 1) & ((1 << ht->bits)-1);
	}
	return (size_t)-1;
}


/* If every one of the following buckets are DELETED (up to the next unused
   one), we can actually mark them all unused. */
static void delete_run(struct hashtable *ht, unsigned int num)
{
	unsigned int i, last = num + 1;

	while (ht->table[last & ((1 << ht->bits)-1)]) {
		if (ptr_is_valid(ht->table[last & ((1 << ht->bits)-1)]))
			return;
		last++;
	}
	for (i = num; i < last; i++)
		ht->table[i & ((1 << ht->bits)-1)] = NULL;
}

void *hashtable_find(struct hashtable *ht,
		     unsigned long hash,
		     bool (*cmp)(const void *htelem, void *cmpdata),
		     void *cmpdata)
{
	size_t i = ht_find(ht, hash, cmp, cmpdata);
	if (i != (size_t)-1)
		return get_raw_ptr(ht->table[i]);
	return NULL;
}

static bool ptr_cmp(const void *htelem, void *cmpdata)
{
	return htelem == cmpdata;
}

bool hashtable_del(struct hashtable *ht, unsigned long hash, const void *p)
{
	size_t i = ht_find(ht, hash, ptr_cmp, (void *)p);
	if (i == (size_t)-1)
		return false;

	ht->elems--;
	ht->table[i] = HASHTABLE_DELETED;
	delete_run(ht, i);
	return true;
}

void _hashtable_traverse(struct hashtable *ht,
			 bool (*cb)(void *p, void *cbarg),
			 void *cbarg)
{
	size_t i;

	for (i = 0; i < (1 << ht->bits); i++) {
		if (ptr_is_valid(ht->table[i]))
			if (cb(get_raw_ptr(ht->table[i]), cbarg))
				return;
	}
}
