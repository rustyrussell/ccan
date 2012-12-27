#ifndef CCAN_ANTITHREAD_NOTIFY_H
#define CCAN_ANTITHREAD_NOTIFY_H
#include <ccan/short_types/short_types.h>
#include <stdbool.h>

/**
 * notify_new - create a notifier.
 * @ctx: the parent context.
 * @addr: the address the notifier is associated with.
 */
struct notify *notify_new(const void *ctx, u32 *addr);

/**
 * notify_recv - receive a notification.
 * @notify: the notifier
 * @val: the value we've seen.
 *
 * Returns true once the contents of the address != val.
 */
bool notify_recv(struct notify *notify, u32 val);

/**
 * notify_send - send a notification.
 * @notify: the notifier.
 *
 * Tells a receiver that we've changed the value at @addr.
 */
bool notify_send(struct notify *notify);

#endif /* CCAN_ANTITHREAD_NOTIFY_H */
