/* Licensed under LGPLv3+ - see LICENSE file for details */
#include <ccan/foreach/foreach.h>
#if !HAVE_COMPOUND_LITERALS || !HAVE_FOR_LOOP_DECLARATION
#include <ccan/list/list.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

/* This list is normally very short. */
static LIST_HEAD(iters);

struct iter_info {
	struct list_node list;
	const void *index;
	unsigned int i, num;
	bool onstack;
};

/* Is pointer still downstack from some other onstack var? */
static bool on_stack(const void *ptr, const void *onstack)
{
#if HAVE_STACK_GROWS_UPWARDS
	return ptr < onstack;
#else
	return ptr > onstack;
#endif
}

static void free_old_iters(const void *index)
{
	struct iter_info *i, *next;

	list_for_each_safe(&iters, i, next, list) {
		/* If we're re-using an index, free the old one.
		 * Otherwise, discard if it's passed off stack. */
		if (i->index == index
		    || (i->onstack && !on_stack(i->index, &i))) {
			list_del(&i->list);
			free(i);
		}
	}
}

static struct iter_info *find_iter(const void *index)
{
	struct iter_info *i;

	list_for_each(&iters, i, list) {
		if (i->index == index)
			return i;
	}
	abort();
}

static struct iter_info *new_iter(const void *index)
{
	struct iter_info *info = malloc(sizeof *info);
	info->index = index;
	info->i = info->num = 0;
	info->onstack = on_stack(index, &info);
	list_add(&iters, &info->list);
	return info;
};

#if HAVE_COMPOUND_LITERALS
void _foreach_iter_init(const void *i)
{
	free_old_iters(i);
	new_iter(i);
}

unsigned int _foreach_iter(const void *i)
{
	struct iter_info *info = find_iter(i);
	return info->i;
}

unsigned int _foreach_iter_inc(const void *i)
{
	struct iter_info *info = find_iter(i);
	return ++info->i;
}
#else /* Don't have compound literals... */
int _foreach_term = 0x42430199;

/* We count values at beginning, and every time around the loop.  We change
 * the terminator each time, so we don't get fooled in case it really appears
 * in the list. */
static unsigned int count_vals(struct iter_info *info, va_list *ap)
{
	unsigned int i;
	int val = 0;

	for (i = 0; i < info->num || val != _foreach_term; i++) {
		val = va_arg(*ap, int);
	}
	_foreach_term++;
	return i;
}

int _foreach_intval_init(const void *i, int val, ...)
{
	va_list ap;
	struct iter_info *info;

	free_old_iters(i);
	info = new_iter(i);

	va_start(ap, val);
	info->num = count_vals(info, &ap);
	va_end(ap);

	return val;
}

bool _foreach_intval_done(const void *i)
{
	struct iter_info *info = find_iter(i);
	return info->i == info->num;
}
	
int _foreach_intval_next(const void *i, int val, ...)
{
	struct iter_info *info = find_iter(i);
	va_list ap;
	unsigned int num;

	va_start(ap, val);
	info->num = count_vals(info, &ap);
	va_end(ap);

	info->i++;
	assert(info->i <= info->num);
	if (info->i == info->num)
		return 0;

	va_start(ap, val);
	for (num = 0; num < info->i; num++)
		val = va_arg(ap, int);

	va_end(ap);
	return val;
}

void *_foreach_ptrval_init(const void *i, const void *val, ...)
{
	free_old_iters(i);
	new_iter(i);

	return (void *)val;
}

void *_foreach_ptrval_next(const void *i, const void *val, ...)
{
	struct iter_info *info = find_iter(i);
	va_list ap;
	unsigned int num;

	info->i++;
	va_start(ap, val);
	for (num = 0; num < info->i; num++)
		val = va_arg(ap, void *);
	va_end(ap);
	return (void *)val;
}
#endif /* !HAVE_COMPOUND_LITERALS */
#endif /* !HAVE_COMPOUND_LITERALS || !HAVE_FOR_LOOP_DECLARATION */
