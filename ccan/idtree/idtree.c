/*
   Based on SAMBA 7ce1356c9f571c55af70bd6b966fe50898c1582d.

   very efficient functions to manage mapping a id (such as a fnum) to
   a pointer. This is used for fnum and search id allocation.

   Copyright (C) Andrew Tridgell 2004

   This code is derived from lib/idr.c in the 2.6 Linux kernel, which was
   written by Jim Houston jim.houston@ccur.com, and is
   Copyright (C) 2002 by Concurrent Computer Corporation

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <ccan/idtree/idtree.h>
#include <ccan/tal/tal.h>
#include <stdint.h>
#include <string.h>

#define IDTREE_BITS 5
#define IDTREE_FULL 0xfffffffful
#if 0 /* unused */
#define TOP_LEVEL_FULL (IDTREE_FULL >> 30)
#endif
#define IDTREE_SIZE (1 << IDTREE_BITS)
#define IDTREE_MASK ((1 << IDTREE_BITS)-1)
#define MAX_ID_SHIFT (sizeof(int)*8 - 1)
#define MAX_ID_BIT (1U << MAX_ID_SHIFT)
#define MAX_ID_MASK (MAX_ID_BIT - 1)
#define MAX_LEVEL (MAX_ID_SHIFT + IDTREE_BITS - 1) / IDTREE_BITS
#define IDTREE_FREE_MAX MAX_LEVEL + MAX_LEVEL

#define set_bit(bit, v) (v) |= (1<<(bit))
#define clear_bit(bit, v) (v) &= ~(1<<(bit))
#define test_bit(bit, v) ((v) & (1<<(bit)))

struct idtree_layer {
	uint32_t		 bitmap;
	struct idtree_layer	*ary[IDTREE_SIZE];
	int			 count;
};

struct idtree {
	struct idtree_layer *top;
	struct idtree_layer *id_free;
	int		  layers;
	int		  id_free_cnt;
};

static struct idtree_layer *alloc_layer(struct idtree *idp)
{
	struct idtree_layer *p;

	if (!(p = idp->id_free))
		return NULL;
	idp->id_free = p->ary[0];
	idp->id_free_cnt--;
	p->ary[0] = NULL;
	return p;
}

static int find_next_bit(uint32_t bm, int maxid, int n)
{
	while (n<maxid && !test_bit(n, bm)) n++;
	return n;
}

static void free_layer(struct idtree *idp, struct idtree_layer *p)
{
	p->ary[0] = idp->id_free;
	idp->id_free = p;
	idp->id_free_cnt++;
}

static int idtree_pre_get(struct idtree *idp)
{
	while (idp->id_free_cnt < IDTREE_FREE_MAX) {
		struct idtree_layer *pn = talz(idp, struct idtree_layer);
		if(pn == NULL)
			return (0);
		free_layer(idp, pn);
	}
	return 1;
}

static int sub_alloc(struct idtree *idp, const void *ptr, int *starting_id)
{
	int n, m, sh;
	struct idtree_layer *p, *pn;
	struct idtree_layer *pa[MAX_LEVEL+1];
	unsigned int l;
	int id, oid;
	uint32_t bm;

	memset(pa, 0, sizeof(pa));

	id = *starting_id;
restart:
	p = idp->top;
	l = idp->layers;
	pa[l--] = NULL;
	while (1) {
		/*
		 * We run around this while until we reach the leaf node...
		 */
		n = (id >> (IDTREE_BITS*l)) & IDTREE_MASK;
		bm = ~p->bitmap;
		m = find_next_bit(bm, IDTREE_SIZE, n);
		if (m == IDTREE_SIZE) {
			/* no space available go back to previous layer. */
			l++;
			oid = id;
			id = (id | ((1 << (IDTREE_BITS*l))-1)) + 1;

			/* if already at the top layer, we need to grow */
			if (!(p = pa[l])) {
				*starting_id = id;
				return -2;
			}

			/* If we need to go up one layer, continue the
			 * loop; otherwise, restart from the top.
			 */
			sh = IDTREE_BITS * (l + 1);
			if (oid >> sh == id >> sh)
				continue;
			else
				goto restart;
		}
		if (m != n) {
			sh = IDTREE_BITS*l;
			id = ((id >> sh) ^ n ^ m) << sh;
		}
		if ((id >= MAX_ID_BIT) || (id < 0))
			return -1;
		if (l == 0)
			break;
		/*
		 * Create the layer below if it is missing.
		 */
		if (!p->ary[m]) {
			if (!(pn = alloc_layer(idp)))
				return -1;
			p->ary[m] = pn;
			p->count++;
		}
		pa[l--] = p;
		p = p->ary[m];
	}
	/*
	 * We have reached the leaf node, plant the
	 * users pointer and return the raw id.
	 */
	p->ary[m] = (struct idtree_layer *)ptr;
	set_bit(m, p->bitmap);
	p->count++;
	/*
	 * If this layer is full mark the bit in the layer above
	 * to show that this part of the radix tree is full.
	 * This may complete the layer above and require walking
	 * up the radix tree.
	 */
	n = id;
	while (p->bitmap == IDTREE_FULL) {
		if (!(p = pa[++l]))
			break;
		n = n >> IDTREE_BITS;
		set_bit((n & IDTREE_MASK), p->bitmap);
	}
	return(id);
}

static int idtree_get_new_above_int(struct idtree *idp,
				    const void *ptr, int starting_id)
{
	struct idtree_layer *p, *pn;
	int layers, v, id;

	idtree_pre_get(idp);

	id = starting_id;
build_up:
	p = idp->top;
	layers = idp->layers;
	if (!p) {
		if (!(p = alloc_layer(idp)))
			return -1;
		layers = 1;
	}
	/*
	 * Add a new layer to the top of the tree if the requested
	 * id is larger than the currently allocated space.
	 */
	while ((layers < MAX_LEVEL) && (id >= (1 << (layers*IDTREE_BITS)))) {
		layers++;
		if (!p->count)
			continue;
		if (!(pn = alloc_layer(idp))) {
			/*
			 * The allocation failed.  If we built part of
			 * the structure tear it down.
			 */
			for (pn = p; p && p != idp->top; pn = p) {
				p = p->ary[0];
				pn->ary[0] = NULL;
				pn->bitmap = pn->count = 0;
				free_layer(idp, pn);
			}
			return -1;
		}
		pn->ary[0] = p;
		pn->count = 1;
		if (p->bitmap == IDTREE_FULL)
			set_bit(0, pn->bitmap);
		p = pn;
	}
	idp->top = p;
	idp->layers = layers;
	v = sub_alloc(idp, ptr, &id);
	if (v == -2)
		goto build_up;
	return(v);
}

static int sub_remove(struct idtree *idp, int shift, int id)
{
	struct idtree_layer *p = idp->top;
	struct idtree_layer **pa[1+MAX_LEVEL];
	struct idtree_layer ***paa = &pa[0];
	int n;

	*paa = NULL;
	*++paa = &idp->top;

	while ((shift > 0) && p) {
		n = (id >> shift) & IDTREE_MASK;
		clear_bit(n, p->bitmap);
		*++paa = &p->ary[n];
		p = p->ary[n];
		shift -= IDTREE_BITS;
	}
	n = id & IDTREE_MASK;
	if (p != NULL && test_bit(n, p->bitmap)) {
		clear_bit(n, p->bitmap);
		p->ary[n] = NULL;
		while(*paa && ! --((**paa)->count)){
			free_layer(idp, **paa);
			**paa-- = NULL;
		}
		if ( ! *paa )
			idp->layers = 0;
		return 0;
	}
	return -1;
}

void *idtree_lookup(const struct idtree *idp, int id)
{
	int n;
	struct idtree_layer *p;

	n = idp->layers * IDTREE_BITS;
	p = idp->top;
	/*
	 * This tests to see if bits outside the current tree are
	 * present.  If so, tain't one of ours!
	 */
	if (n + IDTREE_BITS < 31 &&
	    (id & ~(~0U << MAX_ID_SHIFT)) >> (n + IDTREE_BITS))
	     return NULL;

	/* Mask off upper bits we don't use for the search. */
	id &= MAX_ID_MASK;

	while (n >= IDTREE_BITS && p) {
		n -= IDTREE_BITS;
		p = p->ary[(id >> n) & IDTREE_MASK];
	}
	return((void *)p);
}

bool idtree_remove(struct idtree *idp, int id)
{
	struct idtree_layer *p;

	/* Mask off upper bits we don't use for the search. */
	id &= MAX_ID_MASK;

	if (sub_remove(idp, (idp->layers - 1) * IDTREE_BITS, id) == -1) {
		return false;
	}

	if ( idp->top && idp->top->count == 1 &&
	     (idp->layers > 1) &&
	     idp->top->ary[0]) {
		/* We can drop a layer */
		p = idp->top->ary[0];
		idp->top->bitmap = idp->top->count = 0;
		free_layer(idp, idp->top);
		idp->top = p;
		--idp->layers;
	}
	while (idp->id_free_cnt >= IDTREE_FREE_MAX) {
		p = alloc_layer(idp);
		tal_free(p);
	}
	return true;
}

struct idtree *idtree_new(void *mem_ctx)
{
	return talz(mem_ctx, struct idtree);
}

int idtree_add(struct idtree *idp, const void *ptr, int limit)
{
	int ret = idtree_get_new_above_int(idp, ptr, 0);
	if (ret > limit) {
		idtree_remove(idp, ret);
		return -1;
	}
	return ret;
}

int idtree_add_above(struct idtree *idp, const void *ptr,
		     int starting_id, int limit)
{
	int ret = idtree_get_new_above_int(idp, ptr, starting_id);
	if (ret > limit) {
		idtree_remove(idp, ret);
		return -1;
	}
	return ret;
}
