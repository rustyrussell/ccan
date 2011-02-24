#ifdef CCAN_LIKELY_DEBUG
#include <ccan/likely/likely.h>
#include <ccan/hash/hash.h>
#include <ccan/htable/htable.h>
#include <stdlib.h>
#include <stdio.h>
static struct htable *htable;

struct trace {
	const char *condstr;
	const char *file;
	unsigned int line;
	bool expect;
	unsigned long count, right;
};

/* We hash the pointers, which will be identical for same call. */
static unsigned long hash_trace(const struct trace *trace)
{
	return hash_pointer(trace->condstr,
			    hash_pointer(trace->file,
					 trace->line + trace->expect));
}

static bool hash_cmp(const void *htelem, void *cmpdata)
{
	const struct trace *t1 = htelem, *t2 = cmpdata;
	return t1->condstr == t2->condstr
		&& t1->file == t2->file
		&& t1->line == t2->line
		&& t1->expect == t2->expect;
}

static size_t rehash(const void *elem, void *priv)
{
	return hash_trace(elem);
}

static void init_trace(struct trace *trace,
		       const char *condstr, const char *file, unsigned int line,
		       bool expect)
{
	trace->condstr = condstr;
	trace->file = file;
	trace->line = line;
	trace->expect = expect;
	trace->count = trace->right = 0;
}

static struct trace *add_trace(const char *condstr,
			       const char *file, unsigned int line, bool expect)
{
	struct trace *trace = malloc(sizeof(*trace));
	init_trace(trace, condstr, file, line, expect);
	htable_add(htable, hash_trace(trace), trace);
	return trace;
}

long _likely_trace(bool cond, bool expect,
		   const char *condstr,
		   const char *file, unsigned int line)
{
	struct trace *p, trace;

	if (!htable)
		htable = htable_new(rehash, NULL);

	init_trace(&trace, condstr, file, line, expect);
	p = htable_get(htable, hash_trace(&trace), hash_cmp, &trace);
	if (!p)
		p = add_trace(condstr, file, line, expect);

	p->count++;
	if (cond == expect)
		p->right++;

	return cond;
}

struct get_stats_info {
	struct trace *worst;
	unsigned int min_hits;
	double worst_ratio;
};

static double right_ratio(const struct trace *t)
{
	return (double)t->right / t->count;
}

static void get_stats(struct trace *trace, struct get_stats_info *info)
{
	if (trace->count < info->min_hits)
		return;

	if (right_ratio(trace) < info->worst_ratio) {
		info->worst = trace;
		info->worst_ratio = right_ratio(trace);
	}
}

const char *likely_stats(unsigned int min_hits, unsigned int percent)
{
	struct get_stats_info info;
	struct htable_iter i;
	char *ret;
	struct trace *trace;

	if (!htable)
		return NULL;

	info.min_hits = min_hits;
	info.worst = NULL;
	info.worst_ratio = 2;

	/* This is O(n), but it's not likely called that often. */
	for (trace = htable_first(htable, &i);
	     trace;
	     trace = htable_next(htable,&i)) {
		get_stats(trace, &info);
	}

	if (info.worst_ratio * 100 > percent)
		return NULL;

	ret = malloc(strlen(info.worst->condstr) +
		     strlen(info.worst->file) +
		     sizeof(long int) * 8 +
		     sizeof("%s:%u:%slikely(%s) correct %u%% (%lu/%lu)"));
	sprintf(ret, "%s:%u:%slikely(%s) correct %u%% (%lu/%lu)",
		info.worst->file, info.worst->line,
		info.worst->expect ? "" : "un", info.worst->condstr,
		(unsigned)(info.worst_ratio * 100),
		info.worst->right, info.worst->count);

	htable_del(htable, hash_trace(info.worst), info.worst);
	free(info.worst);

	return ret;
}
#endif /*CCAN_LIKELY_DEBUG*/
