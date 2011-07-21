/* Licensed under GPLv2+ - see LICENSE file for details */
#ifndef TALLOC_LINK_H
#define TALLOC_LINK_H
#include <ccan/talloc/talloc.h>

/**
 * talloc_linked - set up an object with an initial link.
 * @ctx - the context to initially link to
 * @newobj - the newly allocated object (with a NULL parent)
 *
 * The object will be freed when @ctx is freed (or talloc_delink(ctx,
 * newobj) is called), unless more links are added using
 * talloc_link().
 *
 * For convenient chaining, it returns @newobj on success, or frees
 * @newobj and returns NULL.
 */
#define talloc_linked(ctx, newobj) \
	((_TALLOC_TYPEOF(newobj))_talloc_linked((ctx), (newobj)))

/**
 * talloc_link - add another link to a linkable object.
 * @ctx - the context to link to
 * @obj - the object previously made linkable with talloc_linked().
 *
 * The @obj will only be freed when all contexts linked to it are
 * freed (or talloc_delink()ed).
 *
 * Returns @obj, or NULL on failure (out of memory).
 */
#define talloc_link(ctx, obj) \
	((_TALLOC_TYPEOF(obj))_talloc_link((ctx), (obj)))

/**
 * talloc_delink - explicitly remove a link from a linkable object.
 * @ctx - the context previously used for talloc_link/talloc_linked
 * @obj - the object previously used for talloc_link/talloc_linked
 *
 * Explicitly remove a link: normally it is implied by freeing @ctx.
 * Removing the last link frees the object.
 */
void talloc_delink(const void *ctx, const void *linked);

/* Internal helpers. */
void *_talloc_link(const void *ctx, const void *linked);
void *_talloc_linked(const void *ctx, const void *linked);

#endif /* TALLOC_LINK_H */
