/* Licensed under GPLv2+ - see LICENSE file for details */
#include <ccan/list/list.h>
#include <ccan/talloc/talloc.h>
#include <ccan/talloc_link/talloc_link.h>
#include <assert.h>

/* Fake parent, if they care. */
static void *talloc_links = NULL;

/* This is the parent of the linked object, so we can implement delink. */
struct talloc_linked {
	struct list_head links;
	const void *obj;
};

/* This is a child of the linker, but not a parent of ref. */
struct talloc_link {
	struct list_node list;
	struct talloc_linked *linked;
};

static int destroy_link(struct talloc_link *link)
{
	list_del(&link->list);
	if (list_empty(&link->linked->links))
		talloc_free(link->linked);
	return 0;
}

static bool add_link(const void *ctx, struct talloc_linked *linked)
{
	struct talloc_link *link = talloc(ctx, struct talloc_link);
	if (!link)
		return false;

	link->linked = linked;
	list_add(&linked->links, &link->list);
	talloc_set_destructor(link, destroy_link);
	return true;
}

void *_talloc_linked(const void *ctx, const void *newobj)
{
	struct talloc_linked *linked;

	if (talloc_parent(newobj)) {
		/* Assume leak reporting is on: create dummy parent. */
		if (!talloc_links)
			talloc_links = talloc_named_const(NULL, 0,
							  "talloc_links");
		/* This should now have same pseudo-NULL parent. */
		assert(talloc_parent(newobj) == talloc_parent(talloc_links));
	}

	linked = talloc(talloc_links, struct talloc_linked);
	if (!linked) {
		talloc_free(newobj);
		return NULL;
	}
	list_head_init(&linked->links);
	linked->obj = talloc_steal(linked, newobj);

	if (!add_link(ctx, linked)) {
		talloc_free(linked);
		return NULL;
	}

	return (void *)newobj;
}

void *_talloc_link(const void *ctx, const void *obj)
{
	struct talloc_linked *linked;

	linked = talloc_get_type(talloc_parent(obj), struct talloc_linked);
	assert(!list_empty(&linked->links));
	return add_link(ctx, linked) ? (void *)obj : NULL;
}

void talloc_delink(const void *ctx, const void *obj)
{
	struct talloc_linked *linked;
	struct talloc_link *i;

	if (!obj)
		return;

	linked = talloc_get_type(talloc_parent(obj), struct talloc_linked);
	list_for_each(&linked->links, i, list) {
		if (talloc_is_parent(i, ctx)) {
			talloc_free(i);
			return;
		}
	}
	abort();
}
