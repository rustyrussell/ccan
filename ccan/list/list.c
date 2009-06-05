#include <stdio.h>
#include <stdlib.h>
#include "list.h"

struct list_head *list_check(const struct list_head *h, const char *abortstr)
{
	const struct list_node *n, *p;
	int count = 0;

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

	for (p = &h->n, n = h->n.next; n != &h->n; p = n, n = n->next) {
		count++;
		if (n->prev != p) {
			if (!abortstr)
				return NULL;
			fprintf(stderr,
				"%s: prev corrupt in node %p (%u) of %p\n",
				abortstr, n, count, h);
			abort();
		}
	}
	return (struct list_head *)h;
}
