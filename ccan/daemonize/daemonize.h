#ifndef CCAN_DAEMONIZE_H
#define CCAN_DAEMONIZE_H
#include <stdbool.h>

/**
 * daemonize - turn this process into a daemon.
 *
 * This routine forks us off to become a daemon.  It returns false on failure
 * (ie. fork() failed) and sets errno.
 *
 * Side effects for programmers to be aware of:
 *  - PID changes (our parent exits, we become child of init)
 *  - stdin, stdout and stderr file descriptors are closed
 *  - Current working directory changes to /
 *  - Umask is set to 0.
 */
bool daemonize(void);

#endif /* CCAN_DAEMONIZE_H */
