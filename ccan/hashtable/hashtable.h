#ifndef CCAN_HASHTABLE_H
#define CCAN_HASHTABLE_H
#include "config.h"
#include <stdbool.h>

struct hashtable;

/**
 * hashtable_new - allocate a hash tree.
 * @rehash: hash function to use for rehashing.
 * @priv: private argument to @rehash function.
 */
struct hashtable *hashtable_new(unsigned long (*rehash)(const void *elem,
							void *priv),
				void *priv);

/**
 * hashtable_free - dellocate a hash tree.
 *
 * This doesn't do anything to any pointers left in it.
 */
void hashtable_free(struct hashtable *);

/**
 * hashtable_find - look for an object in a hash tree.
 * @ht: the hashtable
 * @hash: the hash value of the object you are seeking
 * @cmp: a comparison function: returns true if the object is found
 * @cmpdata: the data to hand as second arg to @cmp
 *
 * Note that you can do all the work inside the cmp function if you want to:
 * hashtable_find will return @htelem if @cmp returns true.
 */
void *hashtable_find(struct hashtable *ht,
		     unsigned long hash,
		     bool (*cmp)(const void *htelem, void *cmpdata),
		     void *cmpdata);

/**
 * hashtable_add - add an (aligned) pointer into a hash tree.
 * @ht: the hashtable
 * @hash: the hash value of the object
 * @p: the non-NULL pointer, usually from malloc or equiv.
 *
 * Note that this implementation (ab)uses the lower bits of the pointer, so
 * it will abort if the pointer is not aligned to 8 bytes.
 *
 * Also note that this can only fail due to allocation failure.  Otherwise, it
 * returns true.
 */
bool hashtable_add(struct hashtable *ht, unsigned long hash, const void *p);

/**
 * hashtable_del - remove a pointer from a hash tree
 * @ht: the hashtable
 * @hash: the hash value of the object
 * @p: the pointer
 *
 * Returns true if the pointer was found (and deleted).
 */
bool hashtable_del(struct hashtable *ht, unsigned long hash, const void *p);

/**
 * hashtable_traverse - call a function on every pointer in hash tree
 * @ht: the hashtable
 * @cb: the callback: returns true to abort traversal.
 * @cbarg: the argument to the callback
 *
 * Note that your traversal callback may delete any entry (it won't crash),
 * but it may make the traverse unreliable.
 */
void hashtable_traverse(struct hashtable *ht, bool (*cb)(void *p, void *cbarg),
			void *cbarg);
#endif /* CCAN_HASHTABLE_H */
