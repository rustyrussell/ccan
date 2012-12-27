#include "config.h"
#include <ccan/antithread/notify/notify.h>

#if HAVE_FUTEX
#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>

static int futex(void *uaddr, int op, int val)
{
	return syscall(SYS_futex, uaddr, op, val, NULL);
}

struct notify *notify_new(const void *ctx, u32 *addr)
{
	return (struct notify *)addr;
}

bool notify_recv(struct notify *notify, u32 val)
{
	futex(notify, FUTEX_WAIT, val);
	return true;
}

bool notify_send(struct notify *notify)
{
	futex(notify, FUTEX_WAKE, 1);
	return true;
}
#else
#include <unistd.h>
#include <fcntl.h>
#include <ccan/tal/tal.h>

struct notify {
	int fd[2]; /* Read end, then write end. */
	volatile u32 *addr;
};

static void destroy_notify(struct notify *n)
{
	close(n->fd[0]);
	close(n->fd[1]);
}

struct notify *notify_new(const tal_t *ctx, u32 *addr)
{
	struct notify *n = tal(ctx, struct notify);
	if (!n)
		return NULL;
	n->addr = addr;
	if (pipe(n->fd) != 0)
		return tal_free(n);
	if (!tal_add_destructor(n, destroy_notify)) {
		destroy_notify(n);
		return tal_free(n);
	}
	/* Don't block on notify. */
	fcntl(n->fd[1], F_SETFL, fcntl(n->fd[1], F_GETFL)|O_NONBLOCK);
	return n;
}

bool notify_recv(struct notify *notify, u32 val)
{
	char c;

	while (*notify->addr == val) {
		/* Read system call is assumed to be a memory barrier. */
		if (read(notify->fd[0], &c, 1) != 1)
			return false;
	}
	return true;
}

bool notify_send(struct notify *notify)
{
	char c = 0;
	if (write(notify->fd[1], &c, 1) < 0)
		return false;
	return true;
}
#endif /* NO FUTEXES */
