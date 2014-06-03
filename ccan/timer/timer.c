/* LGPL (v2.1 or any later version) - see LICENSE file for details */
#include <ccan/timer/timer.h>
#include <ccan/array_size/array_size.h>
#include <ccan/ilog/ilog.h>
#include <ccan/likely/likely.h>
#include <stdlib.h>
#include <stdio.h>

#define PER_LEVEL (1ULL << TIMER_LEVEL_BITS)

struct timer_level {
	struct list_head list[PER_LEVEL];
};

static uint64_t time_to_grains(struct timeabs t)
{
	return t.ts.tv_sec * ((uint64_t)1000000000 / TIMER_GRANULARITY)
		+ (t.ts.tv_nsec / TIMER_GRANULARITY);
}

static struct timeabs grains_to_time(uint64_t grains)
{
	struct timeabs t;

	t.ts.tv_sec = grains / (1000000000 / TIMER_GRANULARITY);
	t.ts.tv_nsec = (grains % (1000000000 / TIMER_GRANULARITY))
		* TIMER_GRANULARITY;
	return t;
}

void timers_init(struct timers *timers, struct timeabs start)
{
	unsigned int i;

	list_head_init(&timers->far);
	timers->base = time_to_grains(start);
	timers->first = -1ULL;
	for (i = 0; i < ARRAY_SIZE(timers->level); i++)
		timers->level[i] = NULL;
}

static unsigned int level_of(const struct timers *timers, uint64_t time)
{
	uint64_t diff;

	/* Level depends how far away it is. */
	diff = time - timers->base;
	return ilog64(diff / 2) / TIMER_LEVEL_BITS;
}

static void timer_add_raw(struct timers *timers, struct timer *t)
{
	struct list_head *l;
	unsigned int level = level_of(timers, t->time);

	if (!timers->level[level])
		l = &timers->far;
	else {
		int off = (t->time >> (level*TIMER_LEVEL_BITS)) & (PER_LEVEL-1);
		l = &timers->level[level]->list[off];
	}

	list_add_tail(l, &t->list);
}

void timer_add(struct timers *timers, struct timer *t, struct timeabs when)
{
	t->time = time_to_grains(when);

	/* Added in the past?  Treat it as imminent. */
	if (t->time < timers->base)
		t->time = timers->base;
	if (t->time < timers->first)
		timers->first = t->time;

	timer_add_raw(timers, t);
}

/* FIXME: inline */
void timer_del(struct timers *timers, struct timer *t)
{
	list_del(&t->list);
}

static void timers_far_get(struct timers *timers,
			   struct list_head *list,
			   uint64_t when)
{
	struct timer *i, *next;

	list_for_each_safe(&timers->far, i, next, list) {
		if (i->time <= when) {
			list_del_from(&timers->far, &i->list);
			list_add_tail(list, &i->list);
		}
	}
}

static void add_level(struct timers *timers, unsigned int level)
{
	struct timer_level *l;
	struct timer *t;
	unsigned int i;
	struct list_head from_far;

	l = malloc(sizeof(*l));
	if (!l)
		return;

	for (i = 0; i < ARRAY_SIZE(l->list); i++)
		list_head_init(&l->list[i]);
	timers->level[level] = l;

	list_head_init(&from_far);
	timers_far_get(timers, &from_far,
		       timers->base + (1ULL << ((level+1)*TIMER_LEVEL_BITS)) - 1);

	while ((t = list_pop(&from_far, struct timer, list)) != NULL)
		timer_add_raw(timers, t);
}

static const struct timer *find_first(const struct list_head *list,
				      const struct timer *prev)
{
	struct timer *t;

	list_for_each(list, t, list) {
		if (!prev || t->time < prev->time)
			prev = t;
	}
	return prev;
}

static const struct timer *get_first(const struct timers *timers)
{
	unsigned int level, i, off;
	bool need_next;
	uint64_t base;
	const struct timer *found = NULL;
	struct list_head *h;

	if (timers->first < timers->base) {
		base = timers->base;
		level = 0;
	} else {
		/* May not be accurate, due to timer_del / expiry. */
		level = level_of(timers, timers->first);
		base = timers->first >> (TIMER_LEVEL_BITS * level);
	}

next:
	if (!timers->level[level])
		return find_first(&timers->far, NULL);

	need_next = false;
	off = base % PER_LEVEL;
	for (i = 0; i < PER_LEVEL; i++) {
		h = &timers->level[level]->list[(i+off) % PER_LEVEL];

		if (!list_empty(h))
			break;

		/* We haven't cascaded yet, so if we wrap, we'll need to
		 * check next level, too. */
		if (i + off == PER_LEVEL)
			need_next = true;
	}
	if (i == PER_LEVEL) {
		level++;
		base >>= TIMER_LEVEL_BITS;
		goto next;
	}

	/* Level 0 is exact, so they're all the same. */
	if (level == 0)
		found = list_top(h, struct timer, list);
	else
		found = find_first(h, NULL);

	if (need_next) {
		if (!timers->level[level+1]) {
			found = find_first(&timers->far, found);
		} else {
			base >>= TIMER_LEVEL_BITS;
			off = base % PER_LEVEL;
			h = &timers->level[level+1]->list[off];
			found = find_first(h, found);
		}
	}
	return found;
}

static bool update_first(struct timers *timers)
{
	const struct timer *found = get_first(timers);

	if (!found) {
		timers->first = -1ULL;
		return false;
	}

	timers->first = found->time;
	return true;
}

bool timer_earliest(struct timers *timers, struct timeabs *first)
{
	if (!update_first(timers))
		return false;

	*first = grains_to_time(timers->first);
	return true;
}

/* Assume no timers before 'time', cascade down and update base time. */
static void timer_fast_forward(struct timers *timers, uint64_t time)
{
	unsigned int level, changed;
	int need_level = -1;
	struct list_head list;
	struct timer *i;

	/* How many bits changed between base and time?
	 * Each time we wrap, we need to empty buckets from above. */
	if (time == timers->base)
		return;

	changed = ilog64_nz(time ^ timers->base);
	level = (changed - 1) / TIMER_LEVEL_BITS;

	/* Buckets always empty downwards, so we could cascade manually,
	 * but it's rarely very many so we just remove and re-add */
	list_head_init(&list);

	do {
		if (!timers->level[level]) {
			/* We need any which belong on this level. */
			timers_far_get(timers, &list,
				       timers->base
				       + (1ULL << ((level+1)*TIMER_LEVEL_BITS))-1);
			need_level = level;
		} else {
			unsigned src;

			/* Get all timers from this bucket. */
			src = (time >> (level * TIMER_LEVEL_BITS)) % PER_LEVEL;
			list_append_list(&list,
					 &timers->level[level]->list[src]);
		}
	} while (level--);

	/* Did we hit the last level?  If so, add. */
	if (need_level != -1)
		add_level(timers, need_level);

	/* Fast-forward the time, and re-add everyone. */
	timers->base = time;
	while ((i = list_pop(&list, struct timer, list)) != NULL)
		timer_add_raw(timers, i);
}

/* Fills list of expired timers. */
void timers_expire(struct timers *timers,
		   struct timeabs expire,
		   struct list_head *list)
{
	uint64_t now = time_to_grains(expire);
	unsigned int off;

	assert(now >= timers->base);

	list_head_init(list);

	if (!timers->level[0]) {
		if (list_empty(&timers->far))
			return;
		add_level(timers, 0);
	}

	do {
		if (timers->first > now) {
			timer_fast_forward(timers, now);
			break;
		}

		timer_fast_forward(timers, timers->first);
		off = timers->base % PER_LEVEL;

		list_append_list(list, &timers->level[0]->list[off]);
		if (timers->base == now)
			break;
	} while (update_first(timers));
}

static bool timer_list_check(const struct list_head *l,
			     uint64_t min, uint64_t max, uint64_t first,
			     const char *abortstr)
{
	const struct timer *t;

	if (!list_check(l, abortstr))
		return false;

	list_for_each(l, t, list) {
		if (t->time < min || t->time > max) {
			if (abortstr) {
				fprintf(stderr,
					"%s: timer %p %llu not %llu-%llu\n",
					abortstr, t, (long long)t->time,
					(long long)min, (long long)max);
				abort();
			}
			return false;
		}
		if (t->time < first) {
			if (abortstr) {
				fprintf(stderr,
					"%s: timer %p %llu < minimum %llu\n",
					abortstr, t, (long long)t->time,
					(long long)first);
				abort();
			}
			return false;
		}
	}
	return true;
}

struct timers *timers_check(const struct timers *timers, const char *abortstr)
{
	unsigned int l, i, off;
	uint64_t base;

	l = 0;
	if (!timers->level[0])
		goto past_levels;

	/* First level is simple. */
	off = timers->base % PER_LEVEL;
	for (i = 0; i < PER_LEVEL; i++) {
		struct list_head *h;

		h = &timers->level[l]->list[(i+off) % PER_LEVEL];
		if (!timer_list_check(h, timers->base + i, timers->base + i,
				      timers->first, abortstr))
			return NULL;
	}

	/* For other levels, "current" bucket has been emptied, and may contain
	 * entries for the current + level_size bucket. */
	for (l = 1; timers->level[l] && l < PER_LEVEL; l++) {
		uint64_t per_bucket = 1ULL << (TIMER_LEVEL_BITS * l);

		off = ((timers->base >> (l*TIMER_LEVEL_BITS)) % PER_LEVEL);
		/* We start at *next* bucket. */
		base = (timers->base & ~(per_bucket - 1)) + per_bucket;

		for (i = 1; i <= PER_LEVEL; i++) {
			struct list_head *h;

			h = &timers->level[l]->list[(i+off) % PER_LEVEL];
			if (!timer_list_check(h, base, base + per_bucket - 1,
					      timers->first, abortstr))
				return NULL;
			base += per_bucket;
		}
	}

past_levels:
	base = (timers->base & ~((1ULL << (TIMER_LEVEL_BITS * l)) - 1))
		+ (1ULL << (TIMER_LEVEL_BITS * l)) - 1;
	if (!timer_list_check(&timers->far, base, -1ULL, timers->first,
			      abortstr))
		return NULL;

	return (struct timers *)timers;
}

#ifdef CCAN_TIMER_DEBUG
void timers_dump(const struct timers *timers, FILE *fp)
{
	unsigned int l, i;
	uint64_t min, max, num;
	struct timer *t;

	if (!fp)
		fp = stderr;

	fprintf(fp, "Base: %llu\n", timers->base);

	for (l = 0; timers->level[l] && l < ARRAY_SIZE(timers->level); l++) {
		fprintf(fp, "Level %i (+%llu):\n",
			l, (uint64_t)1 << (TIMER_LEVEL_BITS * l));
		for (i = 0; i < (1 << TIMER_LEVEL_BITS); i++) {

			if (list_empty(&timers->level[l]->list[i]))
				continue;
			min = -1ULL;
			max = 0;
			num = 0;
			list_for_each(&timers->level[l]->list[i], t, list) {
				if (t->time < min)
					min = t->time;
				if (t->time > max)
					max = t->time;
				num++;
			}
			fprintf(stderr, "  %llu (+%llu-+%llu)\n",
				num, min - timers->base, max - timers->base);
		}
	}

	min = -1ULL;
	max = 0;
	num = 0;
	list_for_each(&timers->far, t, list) {
		if (t->time < min)
			min = t->time;
		if (t->time > max)
			max = t->time;
		num++;
	}
	fprintf(stderr, "Far: %llu (%llu-%llu)\n", num, min, max);
}
#endif

void timers_cleanup(struct timers *timers)
{
	unsigned int l;

	for (l = 0; l < ARRAY_SIZE(timers->level); l++)
		free(timers->level[l]);
}
