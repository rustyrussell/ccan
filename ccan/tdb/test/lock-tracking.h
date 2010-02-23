#ifndef LOCK_TRACKING_H
#define LOCK_TRACKING_H
#include <stdbool.h>

/* Set this if you want a callback after fnctl unlock. */
extern void (*unlock_callback)(int fd);

/* Replacement fcntl. */
int fcntl_with_lockcheck(int fd, int cmd, ... /* arg */ );

/* Discard locking info: returns number of locks outstanding. */
unsigned int forget_locking(void);

/* Number of errors in locking. */
extern int locking_errors;

/* Suppress lock checking. */
extern bool suppress_lockcheck;
#endif /* LOCK_TRACKING_H */
