/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_INVBLOOM_H
#define CCAN_INVBLOOM_H
#include <ccan/short_types/short_types.h>
#include <ccan/tal/tal.h>

struct invbloom {
	size_t n_elems;
	size_t id_size;
	u32 hashsum_bytes; /* 0 - 4. */
	u32 salt;
	s32 *count; /* [n_elems] */
	u8 *idsum; /* [n_elems][id_size] */
};

/**
 * invbloom_new - create a new invertable bloom lookup table
 * @ctx: context to tal() from, or NULL.
 * @type: type to place into the buckets (must not contain padding)
 * @n_elems: number of entries in table
 * @salt: 32 bit seed for table
 *
 * Returns a new table, which can be freed with tal_free().
 */
#define invbloom_new(ctx, type, n_elems, salt)	\
	invbloom_new_((ctx), sizeof(type), (n_elems), (salt))
struct invbloom *invbloom_new_(const tal_t *ctx,
			       size_t id_size,
			       size_t n_elems, u32 salt);


/**
 * invbloom_insert - add a new element
 * @ib: the invertable bloom lookup table.
 * @elem: the element
 *
 * This is guaranteed to be the inverse of invbloom_delete.
 */
void invbloom_insert(struct invbloom *ib, const void *elem);

/**
 * invbloom_delete - remove an element
 * @ib: the invertable bloom lookup table.
 * @elem: the element
 *
 * Note that this doesn't check the element was previously added (as
 * that can not be done in general anyway).
 *
 * This is guaranteed to be the inverse of invbloom_delete.
 */
void invbloom_delete(struct invbloom *ib, const void *elem);

/**
 * invbloom_get - check if an element is (probably) in the table.
 * @ib: the invertable bloom lookup table.
 * @elem: the element
 *
 * This may return a false negative if the table is too full.
 *
 * It will only return a false positive if deletions and insertions
 * don't match, and have managed to produce a result which matches the
 * element.  This is much less likely.
 */
bool invbloom_get(const struct invbloom *ib, const void *elem);

/**
 * invbloom_extract - try to recover an entry added to a bloom lookup table.
 * @ctx: the context to tal() the return value from.
 * @ib: the invertable bloom lookup table.
 *
 * This may be able to recover (and delete) an element which was
 * invbloom_insert()ed into the table (and not deleted).  This will
 * not work if the table is too full.
 *
 * It may return a bogus element if deletions and insertions don't
 * match.
 */
void *invbloom_extract(const tal_t *ctx, struct invbloom *ib);

/**
 * invbloom_extract_negative - try to recover an entry deleted from a bloom lookup table.
 * @ctx: the context to tal() the return value from.
 * @ib: the invertable bloom lookup table.
 *
 * This may be able to recover (and insert/undo) an element which was
 * invbloom_delete()ed from the table (and not inserted).  This will
 * not work if the table is too full.
 *
 * It may return a bogus element if deletions and insertions don't
 * match.
 */
void *invbloom_extract_negative(const tal_t *ctx, struct invbloom *ib);

/**
 * invbloom_subtract - return the differences of two IBLTs.
 * @ib1: the invertable bloom lookup table to alter
 * @ib2: the invertable bloom lookup table to subtract.
 *
 * This produces exactly the same result as if a new table had all the
 * elements only in @ib1 inserted, then all the elements onlt in @ib2
 * deleted.
 *
 * ie. if @ib1 and @ib2 are similar, the result may be a usable by
 * invbloom_extract and invbloom_extract_negative.
 */
void invbloom_subtract(struct invbloom *ib1, const struct invbloom *ib2);

/**
 * invbloom_empty - is an invertable bloom lookup table completely clean?
 * @ib: the invertable bloom lookup table
 *
 * This is always true if @ib has had the same elements inserted and
 * deleted.  It is far less likely to be true if different ones were
 * deleted than inserted.
 */
bool invbloom_empty(const struct invbloom *ib);
#endif /* CCAN_INVBLOOM_H */
