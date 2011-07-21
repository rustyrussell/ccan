/* Licensed under BSD 3-clause: see LICENSE */
#ifndef CCAN_DAEMON_WITH_NOTIFY_H
#define CCAN_DAEMON_WITH_NOTIFY_H

/**
 * daemonize - turns this process into a daemon
 *
 * This routine will fork() us off to become a daemon. It will return
 * -1 on error and 0 on success.
 *
 * It has a few optional behaviours:
 * @nochdir: if nochdir is set, it won't chdir to /
 *   this means we could hold onto mounts
 * @noclose: if noclose is set, we won't close stdout, stdin and stderr.
 * @wait_sigusr1: if wait_sigusr1 is set, the parent will not exit until the
 *   child has either exited OR it receives a SIGUSR1 signal. You can use this
 *   to have the parent only exit when your process has done all the
 *   danegerous initialization that could cause it to fail to start
 *   (e.g. allocating large amounts of memory, replaying REDO logs).
 *   This allows init scripts starting the daemon to easily report
 *   success/failure.
 */
int daemonize(int nochdir, int noclose, int wait_sigusr1);

/**
 * daemon_is_ready - signals parent that it can exit, we started okay
 *
 * After a daemonize() call, this function will send a SIGUSR1 to the parent
 * telling it to exit as we have started up okay.
 */
int daemon_is_ready(void);

#endif /* CCAN_DAEMON_WITH_NOTIFY_H */
