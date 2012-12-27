#include <ccan/antithread/notify/notify.h>
/* Include the C files directly. */
#include <ccan/antithread/notify/notify.c>
#include <ccan/antithread/antithread.h>
#include <ccan/tap/tap.h>

struct child_info {
	struct notify *n;
	u32 *val;
};

static void *get_notified(struct at_parent *parent, struct child_info *info)
{
	if (!notify_recv(info->n, 0))
		return NULL;
	if (*info->val != 1)
		return NULL;
	(*info->val)++;
	if (!notify_send(info->n))
		return NULL;
	return info;
}

int main(void)
{
	struct child_info info;
	struct at_pool *pool;
	struct at_child *c;

	plan_tests(8);

	pool = at_new_pool(sizeof(info));
	ok1(pool);

	info.val = talz(pool, u32);
	info.n = notify_new(NULL, info.val);

	ok1(info.n);
	c = at_run(pool, get_notified, &info);
	ok1(c);
	*info.val = 1;
	ok1(notify_send(info.n));
	ok1(notify_recv(info.n, 1));
	ok1(*info.val == 2);
	ok1(at_read_child(c) == &info);
	ok1(at_read_child(c) == NULL);
	tal_free(pool);

	return exit_status();
}
