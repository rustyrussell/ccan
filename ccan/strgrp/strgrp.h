/* GNU LGPL - see LICENSE file for details */

#ifndef STRGRP_H
#define STRGRP_H
#include <stdbool.h>

struct strgrp;
struct strgrp_iter;
struct strgrp_grp;
struct strgrp_grp_iter;
struct strgrp_item;

/**
 * Constructs a new strgrp instance.
 * @threshold: A value in [0.0, 1.0] describing the desired similarity of
 *     strings in a cluster
 *
 * @return A heap-allocated strgrp instance, or NULL if initialisation fails.
 * Ownership of the pointer resides with the caller, which must be freed with
 * strgrp_free.
 */
struct strgrp *
strgrp_new(double threshold);

/**
 * Find a group which best matches the provided string key.
 * @ctx: The strgrp instance to search
 * @str: The string key to cluster
 *
 * The returned group is the group providing the maximum score that is equal to
 * or above the configured threshold.
 *
 * @return A matched group, or NULL if no reasonable group is found. Ownership
 * of the returned pointer resides with the strgrp instance and it becomes
 * invalid if the strgrp instance is freed.
 */
const struct strgrp_grp *
strgrp_grp_for(struct strgrp *ctx, const char *str);

/**
 * Add a string key and arbitrary data value (together, an item) to the
 * appropriate group.
 * @ctx: The strgrp instance to add the string and data
 * @str: The string key used to select a group. The caller retains ownership of
 *     the pointer and may free or change the memory prior to freeing the
 *     strgrp instance.
 * @data: The data to attach to the group's new entry. The caller retains
 *     ownership of the pointer, but for correctness its lifetime should be at
 *     least equal to the lifetime of the strgrp instance.
 *
 * Returns the group to which the item was added. Ownership of the returned
 * pointer resides with the strgrp instance and it becomes invalid if the
 * strgrp instance is freed.
 */
const struct strgrp_grp *
strgrp_add(struct strgrp *ctx, const char *str, void *data);

/**
 * Create an iterator over the current groups.
 * @ctx: The strgrp instance to iterate over
 *
 * @return An iterator structure, or NULL if a failure occurred. Ownership of
 * the returned pointer is with the strgrp instance. The caller should pass the
 * pointer strgrp_iter_free when the iterator is exhausted. It is invalid to
 * call strgrp_iter_next or strgrp_iter_free on the returned pointer after the
 * strgrp instance has been freed.
 */
struct strgrp_iter *
strgrp_iter_new(struct strgrp *ctx);

/**
 * Extract the next group from a group iterator
 * @iter: The iterator in question
 *
 * Returns the next group in the iterator or NULL if no further groups exist.
 * Ownership of the returned pointer resides with the strgrp instance and
 * becomes invalid if the strgrp instance is freed.
 */
const struct strgrp_grp *
strgrp_iter_next(struct strgrp_iter *iter);

/**
 * Clean up a group iterator instance
 * @iter: The iterator to free
 */
void
strgrp_iter_free(struct strgrp_iter *iter);

/**
 * Extract the key for a group.
 * @grp: A strgrp_grp pointer
 *
 * A group's key is the input string that caused the creation of the group.
 *
 * Returns the group key. Ownership of the pointer resides with the grp
 * parameter and by extension the strgrp instance. The caller must duplicate
 * the string if the content is required beyond the lifetime of the strgrp
 * instance.
 */
const char *
strgrp_grp_key(const struct strgrp_grp *grp);

/**
 * Create an iterator over items in the provided group
 * @grp: The group whose items to iterate over
 *
 * @return An iterator structure, or NULL if a failure occurred. Ownership of
 * the returned pointer is with the strgrp instance. The caller should pass the
 * pointer strgrp_grp_iter_free when the iterator is exhausted. It is invalid
 * to call strgrp_grp_iter_next or strgrp_grp_iter_free on the returned pointer
 * after the strgrp instance has been freed.
 */
struct strgrp_grp_iter *
strgrp_grp_iter_new(const struct strgrp_grp *grp);

/**
 * Extract the next item from a item iterator
 * @iter: The iterator in question
 *
 * Returns the next group in the iterator or NULL if no further items exist.
 * Ownership of the returned pointer resides with the strgrp instance and
 * becomes invalid if the strgrp instance is freed.
 */
const struct strgrp_item *
strgrp_grp_iter_next(struct strgrp_grp_iter *iter);

/**
 * Clean up an item iterator instance
 * @iter: The iterator to free
 */
void
strgrp_grp_iter_free(struct strgrp_grp_iter *iter);

/**
 * Extract the key for an item
 *
 * @item: The item in question
 *
 * The key is the string input string which generated the item in the cluster.
 *
 * Returns the item key. Ownership of the pointer resides with the item
 * parameter and by extension the strgrp instance. The caller must duplicate
 * the string if the content is required beyond the lifetime of the strgrp
 * instance.
 */
const char *
strgrp_item_key(const struct strgrp_item *item);

/**
 * Extract the value for an item
 * @item: The item in question
 *
 * The value is the arbitrary pointer associated with the input string
 *
 * Returns the item value. The ownership of the pointer does not reside with
 * the strgrp instance, but for correctness should exceed the lifetime of the
 * strgrp instance.
 */
void *
strgrp_item_value(const struct strgrp_item *item);

/**
 * Destroy the strgrp instance
 *
 * @ctx: The strgrp instance in question
 */
void
strgrp_free(struct strgrp *ctx);

/**
 * Destroy the strgrp instance, but not before applying cb() to each item's value element
 * @ctx: The strgrp instance in question
 * @cb: The callback to execute against each item's value. This might be used
 *      to free the value data.
 */
void
strgrp_free_cb(struct strgrp *ctx, void (*cb)(void *data));


/**
 * Dump the groupings to stdout.
 * @ctx: The strgrp instance in question
 */
void
strgrp_print(const struct strgrp *ctx);
#endif
