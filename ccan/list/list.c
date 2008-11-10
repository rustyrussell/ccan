#include <stdio.h>
#include <stdlib.h>
#include "list.h"

struct list_head *list_check(struct list_head *h, const char *abortstr)
{
	struct list_node *n, *p;
	int count = 0;

	if (h->n.next == &h->n) {
		if (h->n.prev != &h->n) {
			if (!abortstr)
				return NULL;
			fprintf(stderr, "%s: prev corrupt in empty %p\n",
				abortstr, h);
			abort();
		}
		return h;
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
	return h;
}
