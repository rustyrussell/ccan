/* GNU LGPL version 2 (or later) - see LICENSE file for details */
#include <assert.h>

#include <ccan/cast/cast.h>

#include <ccan/lpq/lpq.h>

static int lpq_cmp(const struct lpq_ *pq, size_t offset,
		   const struct lpq_link *al, const struct lpq_link *bl)
{
	void *a = (char *)al - offset;
	void *b = (char *)bl - offset;

	return total_order_cmp(pq->order, a, b);
}

struct lpq_link **lpq_frontp_(struct lpq_ *pq, size_t offset)
{
	struct lpq_link **frontp = &pq->list;
	struct lpq_link **p;

	if (lpq_empty_(pq))
		return NULL;

	for (p = &(pq->list->next); *p; p = &(*p)->next) {
		if (lpq_cmp(pq, offset, *p, *frontp) >= 0)
			frontp = p;
	}

	return frontp;
}
