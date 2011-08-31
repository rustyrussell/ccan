#ifndef TDB1_LOCK_TRACKING_H
#define TDB1_LOCK_TRACKING_H
#include <ccan/tdb2/private.h>
#include <stdbool.h>

/* Set this if you want a callback after fnctl unlock. */
extern void (*unlock_callback1)(int fd);

/* Replacement fcntl. */
int fcntl_with_lockcheck1(int fd, int cmd, ... /* arg */ );

/* Discard locking info: returns number of locks outstanding. */
unsigned int forget_locking1(void);

/* Number of errors in locking. */
extern int locking_errors1;

/* Suppress lock checking. */
extern bool suppress_lockcheck1;

/* Make all locks non-blocking. */
extern bool nonblocking_locks1;

/* Number of times we failed a lock because we made it non-blocking. */
extern int locking_would_block1;
#endif /* LOCK_TRACKING_H */
