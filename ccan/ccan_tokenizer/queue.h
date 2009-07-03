/*
        Copyright (c) 2009  Joseph A. Adams
        All rights reserved.
        
        Redistribution and use in source and binary forms, with or without
        modification, are permitted provided that the following conditions
        are met:
        1. Redistributions of source code must retain the above copyright
           notice, this list of conditions and the following disclaimer.
        2. Redistributions in binary form must reproduce the above copyright
           notice, this list of conditions and the following disclaimer in the
           documentation and/or other materials provided with the distribution.
        3. The name of the author may not be used to endorse or promote products
           derived from this software without specific prior written permission.
        
        THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
        IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
        OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
        IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
        INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
        NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
        DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
        THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
        (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
        THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CCAN_QUEUE_H
#define CCAN_QUEUE_H

#include <stdint.h>
#include <ccan/talloc/talloc.h>

#ifndef HAVE_ATTRIBUTE_MAY_ALIAS
#define HAVE_ATTRIBUTE_MAY_ALIAS 1
#endif

#if HAVE_ATTRIBUTE_MAY_ALIAS==1
#define queue_alias(ptr) /* nothing */
#define queue(type) struct {size_t head, tail, flag; type *item;} __attribute__((__may_alias__))
#else
#define queue_alias(ptr) qsort(ptr, 0, 1, queue_alias_helper) //hack
#define queue(type) struct {size_t head, tail, flag; type *item;}
#endif

int queue_alias_helper(const void *a, const void *b);

#define queue_init(queue, ctx) do {(queue).head = (queue).tail = 0; (queue).flag = 3; (queue).item = talloc_size(ctx, sizeof(*(queue).item)*4);} while(0)
#define queue_free(queue) do {talloc_free((queue).item);} while(0)

#define queue_count(queue) (((queue).tail-(queue).head) & (queue).flag)
#define enqueue(queue, ...) \
	do { \
		(queue).item[(queue).tail++] = (__VA_ARGS__); \
		(queue).tail &= (queue).flag; \
		if ((queue).tail == (queue).head) { \
			queue_enqueue_helper(&(queue), sizeof(*(queue).item)); \
			queue_alias(&(queue)); \
		} \
	} while(0)
#define dequeue_check(queue) ((queue).head != (queue).tail ? dequeue(queue) : NULL)
#define dequeue(queue) ((queue).item[queue_dequeue_helper(&(queue).head, (queue).flag)])

//TODO:  Test us
#define queue_next(queue) ((queue).item[(queue).head])
#define queue_item(queue, pos) ((queue).item[((queue).head+(pos)) & (queue).flag])
#define queue_skip(queue) do {(queue).head++; (queue).head &= (queue).flag;} while(0)

void queue_enqueue_helper(void *qp, size_t itemSize);

static inline size_t queue_dequeue_helper(size_t *head, size_t flag) {
	size_t ret = (*head)++;
	*head &= flag;
	return ret;
}

#endif
