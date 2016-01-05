/* Licensed under Apache License v2.0 - see LICENSE file for details */
#include "config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "deque.h"

int deq_resize_(struct deq *q, unsigned n)
{
	char *t;

	assert(q && n > 0 && n >= q->len);

	if (!(t = malloc(q->esz * n)))
		return -1;

	if (q->len) {
		unsigned part1 = q->head + q->len <= q->cap ? q->len : q->cap - q->head;
		unsigned part2 = q->len - part1;
		memcpy(t, q->v + q->head * q->esz, q->esz * part1);
		if (part2)
			memcpy(t + q->esz * part1, q->v, q->esz * part2);
	}
	if (q->cap)
		free(q->v);

	q->v    = t;
	q->head = 0;
	q->tail = q->len;
	q->cap  = n;

	return 0;
}

int deq_op_(struct deq *q, enum deq_op op, unsigned *i)
{
	assert(q && i);
	assert(op == DEQ_PUSH || op == DEQ_POP ||  op == DEQ_SHIFT || op == DEQ_UNSHIFT);

	switch (op) {
	case DEQ_PUSH:
	case DEQ_UNSHIFT:
		if (q->len == q->cap && deq_resize_(q, q->cap == 0 ? q->min : q->cap * 2) == -1)
			return -1;
		break;
	case DEQ_POP:
	case DEQ_SHIFT:
		if (q->cap > q->min) {
			if (q->shrink == DEQ_SHRINK_IF_EMPTY && q->len == 1 && deq_resize_(q, q->min) == -1)
				return -1;
			if (q->shrink == DEQ_SHRINK_AT_20PCT && (q->len - 1) * 5 <= q->cap && deq_resize_(q, q->cap / 2) == -1)
				return -1;
		}
		if (q->len == 0)
			return 0;
	}

	switch (op) {
	case DEQ_PUSH:
		*i = q->tail++;
		q->tail  %= q->cap;
		q->len++;
		break;
	case DEQ_SHIFT:
		*i = q->head++;
		q->head %= q->cap;
		q->len--;
		break;
	case DEQ_POP:
		q->tail = (q->tail == 0 ? q->cap : q->tail) - 1;
		*i = q->tail;
		q->len--;
		break;
	case DEQ_UNSHIFT:
		q->head = (q->head == 0 ? q->cap : q->head) - 1;
		*i = q->head;
		q->len++;
		break;
	}

	return 1;
}

void deq_reset_(struct deq *q)
{
	assert(q);

	if (q->v)
		free(q->v);

	q->v = 0;
	q->head = q->tail = q->len = q->cap = 0;
}
