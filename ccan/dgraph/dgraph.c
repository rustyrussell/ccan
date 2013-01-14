/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include <ccan/dgraph/dgraph.h>
#include <stdlib.h>
#include <stdio.h>

void dgraph_init_node(struct dgraph_node *n)
{
	tlist_init(&n->edge[DGRAPH_FROM]);
	tlist_init(&n->edge[DGRAPH_TO]);
}

static void free_edge(struct dgraph_edge *e)
{
	tlist_del_from(&e->n[DGRAPH_FROM]->edge[DGRAPH_FROM],
		       e, list[DGRAPH_FROM]);
	tlist_del_from(&e->n[DGRAPH_TO]->edge[DGRAPH_TO],
		       e, list[DGRAPH_TO]);
	free(e);
}

void dgraph_clear_node(struct dgraph_node *n)
{
	struct dgraph_edge *e;
	unsigned int i;

	(void)dgraph_debug(n);
	for (i = DGRAPH_FROM; i <= DGRAPH_TO; i++) {
		while ((e = tlist_top(&n->edge[i], list[i])) != NULL) {
			assert(e->n[i] == n);
			free_edge(e);
		}
	}
}

void dgraph_add_edge(struct dgraph_node *from, struct dgraph_node *to)
{
	struct dgraph_edge *e = malloc(sizeof(*e));

	(void)dgraph_debug(from);
	(void)dgraph_debug(to);
	e->n[DGRAPH_FROM] = from;
	e->n[DGRAPH_TO] = to;
	tlist_add(&from->edge[DGRAPH_FROM], e, list[DGRAPH_FROM]);
	tlist_add(&to->edge[DGRAPH_TO], e, list[DGRAPH_TO]);
}

bool dgraph_del_edge(struct dgraph_node *from, struct dgraph_node *to)
{
	struct dgraph_edge *e, *next;

	(void)dgraph_debug(from);
	(void)dgraph_debug(to);
	dgraph_for_each_edge_safe(from, e, next, DGRAPH_FROM) {
		if (e->n[DGRAPH_TO] == to) {
			free_edge(e);
			return true;
		}
	}
	return false;
}

static bool traverse_depth_first(struct dgraph_node *n,
				 enum dgraph_dir dir,
				 bool (*fn)(struct dgraph_node *, void *),
				 const void *data)
{
	struct dgraph_edge *e, *next;

	/* dgraph_for_each_edge_safe, without dgraph_debug() */
	tlist_for_each_safe(&n->edge[dir], e, next, list[dir]) {
		if (!traverse_depth_first(e->n[!dir], dir, fn, data))
			return false;
	}
	return fn(n, (void *)data);
}

void dgraph_traverse(const struct dgraph_node *n,
		     enum dgraph_dir dir,
		     bool (*fn)(struct dgraph_node *, void *),
		     const void *data)
{
	struct dgraph_edge *e, *next;

	/* dgraph_for_each_edge_safe, without dgraph_debug() */
	tlist_for_each_safe(&n->edge[dir], e, next, list[dir]) {
		if (!traverse_depth_first(e->n[!dir], dir, fn, data))
			break;
	}
}

struct check_info {
	const struct dgraph_node *ret;
	const char *abortstr;
};

static bool find_backedge(const struct dgraph_node *from,
			  const struct dgraph_node *to,
			  enum dgraph_dir dir)
{
	struct dgraph_edge *e;

	tlist_for_each(&from->edge[dir], e, list[dir]) {
		if (e->n[!dir] == to)
			return true;
	}
	return false;
}

static bool dgraph_check_node(struct dgraph_node *n, void *info_)
{
	struct check_info *info = info_;
	unsigned int dir;
	struct dgraph_edge *e;

	for (dir = DGRAPH_FROM; dir <= DGRAPH_TO; dir++) {
		/* First, check edges list. */
		if (!tlist_check(&n->edge[dir], info->abortstr)) {
			info->ret = NULL;
			return false;
		}

		/* dgraph_for_each_edge() without check! */
		tlist_for_each(&n->edge[dir], e, list[dir]) {
			if (e->n[dir] == n) {
				if (find_backedge(e->n[!dir], n, !dir))
					continue;
				if (info->abortstr) {
					fprintf(stderr,
						"%s: node %p %s edge doesnt"
						" point back to %p\n",
						info->abortstr, e->n[!dir],
						!dir == DGRAPH_FROM
						? "DGRAPH_FROM" : "DGRAPH_TO",
						n);
					abort();
				}
				info->ret = NULL;
				return false;
			}

			if (info->abortstr) {
				fprintf(stderr,
					"%s: node %p %s edge %p points"
					" to %p\n",
					info->abortstr, n,
					dir == DGRAPH_FROM
					? "DGRAPH_FROM" : "DGRAPH_TO",
					e, e->n[dir]);
				abort();
			}
			info->ret = NULL;
			return false;
		}
	}

	return true;
}

struct dgraph_node *dgraph_check(const struct dgraph_node *n,
				 const char *abortstr)
{
	struct check_info info;

	/* This gets set to NULL by dgraph_check_node on failure. */
	info.ret = n;
	info.abortstr = abortstr;

	dgraph_check_node((struct dgraph_node *)info.ret, &info);

	if (info.ret)
		dgraph_traverse(n, DGRAPH_FROM, dgraph_check_node, &info);
	if (info.ret)
		dgraph_traverse(n, DGRAPH_TO, dgraph_check_node, &info);
	return (void *)info.ret;
}
