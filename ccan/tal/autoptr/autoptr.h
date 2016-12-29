/* MIT (BSD) license - see LICENSE file for details */
#ifndef CCAN_TAL_AUTOPTR_H
#define CCAN_TAL_AUTOPTR_H
#include <ccan/tal/tal.h>

struct autonull;

/**
 * autonull_set_ptr - set a pointer, NULL it when pointer tal_free'd.
 * @ctx: the tal context which owns this autonull.
 * @pp: pointer to tal pointer
 * @p: the tal pointer to set *@pp to.
 *
 * *@pp is set to @p.  When @p is tal_free'd directly or indirectly, *@pp will
 * be set to NULL.  Or, if the returned object is freed, the callback is
 * deactivated.
 *
 * Example:
 *	struct parent {
 *		struct child *c;
 *	};
 *	struct child {
 *		const char *name;
 *	};
 *
 *	int main(void)
 *	{
 *		struct parent *p = tal(NULL, struct parent);
 *		struct child *c = tal(p, struct child);
 *		c->name = "Child";
 *
 *		autonull_set_ptr(p, &p->c, c);
 *		assert(p->c == c);
 *
 *		// Automatically clears p->c.
 *		tal_free(c);
 *		assert(p->c == NULL);
 *		return 0;
 *	}
 *
 */
#define autonull_set_ptr(ctx, pp, p)		\
	autonull_set_ptr_((ctx), (pp) + 0*sizeof(*(pp) = (p)), (p))

struct autonull *autonull_set_ptr_(const tal_t *ctx, void *pp, const tal_t *p);

#endif /* CCAN_TAL_AUTOPTR_H */
