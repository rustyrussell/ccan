#include <stdio.h>
#include <stdlib.h>
#include "list.h"

struct list_node *list_check_node(const struct list_node *node,
				  const char *abortstr)
{
	const struct list_node *p, *n;
	int count = 0;

	for (p = node, n = node->next; n != node; p = n, n = n->next) {
		count++;
		if (n->prev != p) {
			if (!abortstr)
				return NULL;
			fprintf(stderr,
				"%s: prev corrupt in node %p (%u) of %p\n",
				abortstr, n, count, node);
			abort();
		}
	}
	return (struct list_node *)node;
}

struct list_head *list_check(const struct list_head *h, const char *abortstr)
{
	if (h->n.next == &h->n) {
		if (h->n.prev != &h->n) {
			if (!abortstr)
				return NULL;
			fprintf(stderr, "%s: prev corrupt in empty %p\n",
				abortstr, h);
			abort();
		}
		return (struct list_head *)h;
	}

	if (!list_check_node(&h->n, abortstr))
		return NULL;
	return (struct list_head *)h;
}
