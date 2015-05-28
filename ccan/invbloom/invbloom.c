/* Licensed under BSD-MIT - see LICENSE file for details */
#include "invbloom.h"
#include <ccan/hash/hash.h>
#include <ccan/endian/endian.h>
#include <assert.h>

/* 	"We will show that hash_count values of 3 or 4 work well in practice"

	From:

	Eppstein, David, et al. "What's the difference?: efficient set reconciliation without prior context." ACM SIGCOMM Computer Communication Review. Vol. 41. No. 4. ACM, 2011. http://conferences.sigcomm.org/sigcomm/2011/papers/sigcomm/p218.pdf
*/
#define NUM_HASHES 3

struct invbloom *invbloom_new_(const tal_t *ctx,
			       size_t id_size,
			       size_t n_elems,
			       u32 salt)
{
	struct invbloom *ib = tal(ctx, struct invbloom);

	if (ib) {
		ib->n_elems = n_elems;
		ib->id_size = id_size;
		ib->salt = salt;
		ib->singleton = NULL;
		ib->count = tal_arrz(ib, s32, n_elems);
		ib->idsum = tal_arrz(ib, u8, id_size * n_elems);
		if (!ib->count || !ib->idsum)
			ib = tal_free(ib);
	}
	return ib;
}

void invbloom_singleton_cb_(struct invbloom *ib,
			    void (*cb)(struct invbloom *,
				       size_t bucket, bool, void *),
			    void *data)
{
	ib->singleton = cb;
	ib->singleton_data = data;
}

static size_t hash_bucket(const struct invbloom *ib, const void *id, size_t i)
{
	return hash((const char *)id, ib->id_size, ib->salt+i*7) % ib->n_elems;
}

static u8 *idsum_ptr(const struct invbloom *ib, size_t bucket)
{
	return (u8 *)ib->idsum + bucket * ib->id_size;
}

static void check_for_singleton(struct invbloom *ib, size_t bucket, bool before)
{
	if (!ib->singleton)
		return;

	if (ib->count[bucket] != 1 && ib->count[bucket] != -1)
		return;

	ib->singleton(ib, bucket, before, ib->singleton_data);
}

static void add_to_bucket(struct invbloom *ib, size_t n, const u8 *id)
{
	size_t i;
	u8 *idsum = idsum_ptr(ib, n);
	
	check_for_singleton(ib, n, true);

	ib->count[n]++;

	for (i = 0; i < ib->id_size; i++)
		idsum[i] ^= id[i];

	check_for_singleton(ib, n, false);
}

static void remove_from_bucket(struct invbloom *ib, size_t n, const u8 *id)
{
	size_t i;
	u8 *idsum = idsum_ptr(ib, n);

	check_for_singleton(ib, n, true);

	ib->count[n]--;
	for (i = 0; i < ib->id_size; i++)
		idsum[i] ^= id[i];

	check_for_singleton(ib, n, false);
}

void invbloom_insert(struct invbloom *ib, const void *id)
{
	unsigned int i;

	for (i = 0; i < NUM_HASHES; i++)
		add_to_bucket(ib, hash_bucket(ib, id, i), id);
}

void invbloom_delete(struct invbloom *ib, const void *id)
{
	unsigned int i;

	for (i = 0; i < NUM_HASHES; i++)
		remove_from_bucket(ib, hash_bucket(ib, id, i), id);
}

static bool all_zero(const u8 *mem, size_t size)
{
	unsigned int i;

	for (i = 0; i < size; i++)
		if (mem[i])
			return false;
	return true;
}

bool invbloom_get(const struct invbloom *ib, const void *id)
{
	unsigned int i;

	for (i = 0; i < NUM_HASHES; i++) {
		size_t h = hash_bucket(ib, id, i);
		u8 *idsum = idsum_ptr(ib, h);

		if (ib->count[h] == 0 && all_zero(idsum, ib->id_size))
			return false;

		if (ib->count[h] == 1)
			return (memcmp(idsum, id, ib->id_size) == 0);
	}
	return false;
}

static void *extract(const tal_t *ctx, struct invbloom *ib, int count)
{
	size_t i;

	/* FIXME: this makes full extraction O(n^2). */
	for (i = 0; i < ib->n_elems; i++) {
		void *id;

		if (ib->count[i] != count)
			continue;

		id = tal_dup_arr(ctx, u8, idsum_ptr(ib, i), ib->id_size, 0);
		return id;
	}
	return NULL;
}

void *invbloom_extract(const tal_t *ctx, struct invbloom *ib)
{
	void *id;

	id = extract(ctx, ib, 1);
	if (id)
		invbloom_delete(ib, id);
	return id;
}

void *invbloom_extract_negative(const tal_t *ctx, struct invbloom *ib)
{
	void *id;

	id = extract(ctx, ib, -1);
	if (id)
		invbloom_insert(ib, id);
	return id;
}

void invbloom_subtract(struct invbloom *ib1, const struct invbloom *ib2)
{
	size_t i;

	assert(ib1->n_elems == ib2->n_elems);
	assert(ib1->id_size == ib2->id_size);
	assert(ib1->salt == ib2->salt);

	for (i = 0; i < ib1->n_elems; i++)
		check_for_singleton(ib1, i, true);

	for (i = 0; i < ib1->n_elems * ib1->id_size; i++)
		ib1->idsum[i] ^= ib2->idsum[i];

	for (i = 0; i < ib1->n_elems; i++) {
		ib1->count[i] -= ib2->count[i];
		check_for_singleton(ib1, i, false);
	}
}

bool invbloom_empty(const struct invbloom *ib)
{
	size_t i;

	for (i = 0; i < ib->n_elems; i++) {
		if (ib->count[i])
			return false;
		if (!all_zero(idsum_ptr(ib, i), ib->id_size))
			return false;
	}
	return true;
}
