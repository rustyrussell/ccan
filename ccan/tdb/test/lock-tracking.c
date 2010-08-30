/* We save the locks so we can reaquire them. */
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ccan/tap/tap.h>
#include <ccan/tdb/tdb_private.h>

struct lock {
	struct lock *next;
	unsigned int off;
	unsigned int len;
	int type;
};
static struct lock *locks;
int locking_errors = 0;
bool suppress_lockcheck = false;
void (*unlock_callback)(int fd);

int fcntl_with_lockcheck(int fd, int cmd, ... /* arg */ )
{
	va_list ap;
	int ret, arg3;
	struct flock *fl;

	if (cmd != F_SETLK && cmd != F_SETLKW) {
		/* This may be totally bogus, but we don't know in general. */
		va_start(ap, cmd);
		arg3 = va_arg(ap, int);
		va_end(ap);

		return fcntl(fd, cmd, arg3);
	}

	va_start(ap, cmd);
	fl = va_arg(ap, struct flock *);
	va_end(ap);

	if (fl->l_type == F_UNLCK) {
		struct lock **l;
		struct lock *old = NULL;
			
		for (l = &locks; *l; l = &(*l)->next) {
			if ((*l)->off == fl->l_start
			    && (*l)->len == fl->l_len) {
				old = *l;
				*l = (*l)->next;
				free(old);
				break;
			}
		}
		if (!old && !suppress_lockcheck) {
			diag("Unknown unlock %u@%u",
			     (int)fl->l_len, (int)fl->l_start);
			locking_errors++;
		}
	} else {
		struct lock *new, *i;
		unsigned int fl_end = fl->l_start + fl->l_len;
		if (fl->l_len == 0)
			fl_end = (unsigned int)-1;

		/* Check for overlaps: we shouldn't do this. */
		for (i = locks; i; i = i->next) {
			unsigned int i_end = i->off + i->len;
			if (i->len == 0)
				i_end = (unsigned int)-1;

			if (fl->l_start >= i->off && fl->l_start < i_end)
				break;
			if (fl_end >= i->off && fl_end < i_end)
				break;

			/* tdb_allrecord_lock does this, handle adjacent: */
			if (fl->l_start == i_end && fl->l_type == i->type) {
				i->len = fl->l_len ? i->len + fl->l_len : 0;
				goto ok;
			}
		}
		if (i) {
			/* Special case: upgrade of allrecord lock. */
			if (i->type == F_RDLCK && fl->l_type == F_WRLCK
			    && i->off == FREELIST_TOP
			    && fl->l_start == FREELIST_TOP
			    && i->len == 0
			    && fl->l_len == 0) {
				i->type = F_WRLCK;
				goto ok;
			}
			if (!suppress_lockcheck) {
				diag("%s lock %u@%u overlaps %u@%u",
				     fl->l_type == F_WRLCK ? "write" : "read",
				     (int)fl->l_len, (int)fl->l_start,
				     i->len, (int)i->off);
				locking_errors++;
			}
		}
		new = malloc(sizeof *new);
		new->off = fl->l_start;
		new->len = fl->l_len;
		new->type = fl->l_type;
		new->next = locks;
		locks = new;
	}
ok:
	ret = fcntl(fd, cmd, fl);
	if (ret == 0 && fl->l_type == F_UNLCK && unlock_callback)
		unlock_callback(fd);
	return ret;
}

int forget_locking(void)
{
	unsigned int num = 0;
	while (locks) {
		struct lock *next = locks->next;
		free(locks);
		locks = next;
		num++;
	}
	return num;
}
