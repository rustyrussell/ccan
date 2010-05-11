#ifdef DEBUG
#include <ccan/likely/likely.h>
#include <ccan/hash/hash.h>
#include <ccan/hashtable/hashtable.h>
#include <stdlib.h>
#include <stdio.h>
static struct hashtable *htable;

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

static unsigned long rehash(const void *elem, void *priv)
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
	hashtable_add(htable, hash_trace(trace), trace);
	return trace;
}

long _likely_trace(bool cond, bool expect,
		   const char *condstr,
		   const char *file, unsigned int line)
{
	struct trace *p, trace;

	if (!htable)
		htable = hashtable_new(rehash, NULL);

	init_trace(&trace, condstr, file, line, expect);
	p = hashtable_find(htable, hash_trace(&trace), hash_cmp, &trace);
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

static bool get_stats(void *elem, void *vinfo)
{
	struct trace *trace = elem;
	struct get_stats_info *info = vinfo;

	if (trace->count < info->min_hits)
		return false;

	if (right_ratio(trace) < info->worst_ratio) {
		info->worst = trace;
		info->worst_ratio = right_ratio(trace);
	}
	return false;
}

const char *likely_stats(unsigned int min_hits, unsigned int percent)
{
	struct get_stats_info info;
	char *ret;

	if (!htable)
		return NULL;

	info.min_hits = min_hits;
	info.worst = NULL;
	info.worst_ratio = 2;

	/* This is O(n), but it's not likely called that often. */
	hashtable_traverse(htable, get_stats, &info);

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

	hashtable_del(htable, hash_trace(info.worst), info.worst);
	free(info.worst);

	return ret;
}
#endif /*DEBUG*/
