/* MIT (BSD) license - see LICENSE file for details */
#include <ccan/tal/autoptr/autoptr.h>

struct autonull {
	void **pp;
};

static void autonull_remove(struct autonull *a);
static void autonull_null_out(tal_t *p UNNEEDED, struct autonull *a)
{
	void **pp = a->pp;
	tal_del_destructor(a, autonull_remove);
	tal_free(a);
	*pp = NULL;
}

static void autonull_remove(struct autonull *a)
{
	/* Don't NULL us out now. */
	tal_del_destructor2(*a->pp, autonull_null_out, a);
}

struct autonull *autonull_set_ptr_(const tal_t *ctx, void *pp, const tal_t *p)
{
	struct autonull *a = tal(ctx, struct autonull);
	a->pp = (void **)pp;
	*a->pp = (void *)p;

	/* If p is freed, NULL out a->pp */
	tal_add_destructor2(*a->pp, autonull_null_out, a);

	/* If they free autonull, it removes other destructor. */
	tal_add_destructor(a, autonull_remove);
	return a;
}
