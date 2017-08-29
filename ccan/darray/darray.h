/*
 * Copyright (C) 2011 Joseph Adams <joeyadams3.14159@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef CCAN_DARRAY_H
#define CCAN_DARRAY_H

#include <stdlib.h>
#include <string.h>
#include "config.h"

/*
 * SYNOPSIS
 *
 * Life cycle of a darray (dynamically-allocated array):
 *
 *     darray(int) a = darray_new();
 *     darray_free(a);
 *
 *     struct {darray(int) a;} foo;
 *     darray_init(foo.a);
 *     darray_free(foo.a);
 *
 * Typedefs for darrays of common types:
 *
 *     darray_char, darray_schar, darray_uchar
 *     darray_short, darray_int, darray_long
 *     darray_ushort, darray_uint, darray_ulong
 *
 * Access:
 *
 *     T      darray_item(darray(T) arr, size_t index);
 *     size_t darray_size(darray(T) arr);
 *     size_t darray_alloc(darray(T) arr);
 *     bool   darray_empty(darray(T) arr);
 *
 * Insertion (single item):
 *
 *     void   darray_append(darray(T) arr, T item);
 *     void   darray_prepend(darray(T) arr, T item);
 *     void   darray_insert(darray(T) arr, size_t index, T item);
 *     void   darray_push(darray(T) arr, T item); // same as darray_append
 *
 * Insertion (multiple items):
 *
 *     void   darray_append_items(darray(T) arr, T *items, size_t count);
 *     void   darray_prepend_items(darray(T) arr, T *items, size_t count);
 *
 *     void   darray_appends(darray(T) arr, [T item, [...]]);
 *     void   darray_prepends(darray(T) arr, [T item, [...]]);
 *
 *     // Same functionality as above, but does not require typeof.
 *     void   darray_appends_t(darray(T) arr, #T, [T item, [...]]);
 *     void   darray_prepends_t(darray(T) arr, #T, [T item, [...]]);
 *
 * Removal:
 *
 *     T      darray_pop(darray(T) arr | darray_size(arr) != 0);
 *     T*     darray_pop_check(darray(T*) arr);
 *     void   darray_remove(darray(T) arr, size_t index);
 *
 * Replacement:
 *
 *     void   darray_from_items(darray(T) arr, T *items, size_t count);
 *     void   darray_from_c(darray(T) arr, T c_array[N]);
 *
 * String buffer:
 *
 *     void   darray_append_string(darray(char) arr, const char *str);
 *     void   darray_append_lit(darray(char) arr, char stringLiteral[N+1]);
 *
 *     void   darray_prepend_string(darray(char) arr, const char *str);
 *     void   darray_prepend_lit(darray(char) arr, char stringLiteral[N+1]);
 *
 *     void   darray_from_string(darray(T) arr, const char *str);
 *     void   darray_from_lit(darray(char) arr, char stringLiteral[N+1]);
 *
 * Size management:
 *
 *     void   darray_resize(darray(T) arr, size_t newSize);
 *     void   darray_resize0(darray(T) arr, size_t newSize);
 *
 *     void   darray_realloc(darray(T) arr, size_t newAlloc);
 *     void   darray_growalloc(darray(T) arr, size_t newAlloc);
 *
 *     void   darray_make_room(darray(T) arr, size_t room);
 *
 * Traversal:
 *
 *     darray_foreach(T *&i, darray(T) arr) {...}
 *     darray_foreach_reverse(T *&i, darray(T) arr) {...}
 *
 * Except for darray_foreach, darray_foreach_reverse, and darray_remove,
 * all macros evaluate their non-darray arguments only once.
 */

/*** Life cycle ***/

#define darray(type) struct {type *item; size_t size; size_t alloc;}

#define darray_new() {0,0,0}
#define darray_init(arr) do {(arr).item=0; (arr).size=0; (arr).alloc=0;} while(0)
#define darray_free(arr) do {free((arr).item);} while(0)


/*
 * Typedefs for darrays of common types.  These are useful
 * when you want to pass a pointer to an darray(T) around.
 *
 * The following will produce an incompatible pointer warning:
 *
 *     void foo(darray(int) *arr);
 *     darray(int) arr = darray_new();
 *     foo(&arr);
 *
 * The workaround:
 *
 *     void foo(darray_int *arr);
 *     darray_int arr = darray_new();
 *     foo(&arr);
 */

typedef darray(char)           darray_char;
typedef darray(signed char)    darray_schar;
typedef darray(unsigned char)  darray_uchar;

typedef darray(short)          darray_short;
typedef darray(int)            darray_int;
typedef darray(long)           darray_long;

typedef darray(unsigned short) darray_ushort;
typedef darray(unsigned int)   darray_uint;
typedef darray(unsigned long)  darray_ulong;


/*** Access ***/

#define darray_item(arr, i) ((arr).item[i])
#define darray_size(arr)    ((arr).size)
#define darray_alloc(arr)   ((arr).alloc)
#define darray_empty(arr)   ((arr).size == 0)


/*** Insertion (single item) ***/

#define darray_append(arr, ...) do { \
		darray_resize(arr, (arr).size+1); \
		(arr).item[(arr).size-1] = (__VA_ARGS__); \
	} while(0)
#define darray_prepend(arr, ...) do { \
		darray_resize(arr, (arr).size+1); \
		memmove((arr).item+1, (arr).item, ((arr).size-1)*sizeof(*(arr).item)); \
		(arr).item[0] = (__VA_ARGS__); \
	} while(0)
#define darray_insert(arr, i, ...) do { \
		size_t index_ = (i); \
		darray_resize(arr, (arr).size+1); \
		memmove((arr).item+index_+1, (arr).item+index_, ((arr).size-index_-1)*sizeof(*(arr).item)); \
		(arr).item[index_] = (__VA_ARGS__); \
	} while(0)
#define darray_push(arr, ...) darray_append(arr, __VA_ARGS__)


/*** Insertion (multiple items) ***/

#define darray_append_items(arr, items, count) do { \
		size_t count_ = (count), oldSize_ = (arr).size; \
		darray_resize(arr, oldSize_ + count_); \
		memcpy((arr).item + oldSize_, items, count_ * sizeof(*(arr).item)); \
	} while(0)

#define darray_prepend_items(arr, items, count) do { \
		size_t count_ = (count), oldSize_ = (arr).size; \
		darray_resize(arr, count_ + oldSize_); \
		memmove((arr).item + count_, (arr).item, oldSize_ * sizeof(*(arr).item)); \
		memcpy((arr).item, items, count_ * sizeof(*(arr).item)); \
	} while(0)

#define darray_append_items_nullterminate(arr, items, count) do { \
		size_t count_ = (count), oldSize_ = (arr).size; \
		darray_resize(arr, oldSize_ + count_ + 1); \
		memcpy((arr).item + oldSize_, items, count_ * sizeof(*(arr).item)); \
		(arr).item[--(arr).size] = 0; \
	} while(0)

#define darray_prepend_items_nullterminate(arr, items, count) do { \
		size_t count_ = (count), oldSize_ = (arr).size; \
		darray_resize(arr, count_ + oldSize_ + 1); \
		memmove((arr).item + count_, (arr).item, oldSize_ * sizeof(*(arr).item)); \
		memcpy((arr).item, items, count_ * sizeof(*(arr).item)); \
		(arr).item[--(arr).size] = 0; \
	} while(0)

#if HAVE_TYPEOF
#define darray_appends(arr, ...) darray_appends_t(arr, typeof((*(arr).item)), __VA_ARGS__)
#define darray_prepends(arr, ...) darray_prepends_t(arr, typeof((*(arr).item)), __VA_ARGS__)
#endif

#define darray_appends_t(arr, type, ...) do { \
		type src_[] = {__VA_ARGS__}; \
		darray_append_items(arr, src_, sizeof(src_)/sizeof(*src_)); \
	} while(0)
#define darray_prepends_t(arr, type, ...) do { \
		type src_[] = {__VA_ARGS__}; \
		darray_prepend_items(arr, src_, sizeof(src_)/sizeof(*src_)); \
	} while(0)


/*** Removal ***/

/* Warning: Do not call darray_pop on an empty darray. */
#define darray_pop(arr) ((arr).item[--(arr).size])
#define darray_pop_check(arr) ((arr).size ? darray_pop(arr) : NULL)
/* Warning, slow: Requires copying all elements after removed item. */
#define darray_remove(arr, i) do { \
	size_t index_ = (i); \
	if (index_ < arr.size-1)    \
		memmove(&(arr).item[index_], &(arr).item[index_+1], ((arr).size-1-index_)*sizeof(*(arr).item)); \
	(arr).size--;  \
	} while(0)


/*** Replacement ***/

#define darray_from_items(arr, items, count) do {size_t count_ = (count); darray_resize(arr, count_); memcpy((arr).item, items, count_*sizeof(*(arr).item));} while(0)
#define darray_from_c(arr, c_array) darray_from_items(arr, c_array, sizeof(c_array)/sizeof(*(c_array)))


/*** String buffer ***/

#define darray_append_string(arr, str) do {const char *str_ = (str); darray_append_items(arr, str_, strlen(str_)+1); (arr).size--;} while(0)
#define darray_append_lit(arr, stringLiteral) do {darray_append_items(arr, stringLiteral, sizeof(stringLiteral)); (arr).size--;} while(0)

#define darray_prepend_string(arr, str) do { \
		const char *str_ = (str); \
		darray_prepend_items_nullterminate(arr, str_, strlen(str_)); \
	} while(0)
#define darray_prepend_lit(arr, stringLiteral) \
	darray_prepend_items_nullterminate(arr, stringLiteral, sizeof(stringLiteral) - 1)

#define darray_from_string(arr, str) do {const char *str_ = (str); darray_from_items(arr, str_, strlen(str_)+1); (arr).size--;} while(0)
#define darray_from_lit(arr, stringLiteral) do {darray_from_items(arr, stringLiteral, sizeof(stringLiteral)); (arr).size--;} while(0)


/*** Size management ***/

#define darray_resize(arr, newSize) darray_growalloc(arr, (arr).size = (newSize))
#define darray_resize0(arr, newSize) do { \
		size_t oldSize_ = (arr).size, newSize_ = (newSize); \
		(arr).size = newSize_; \
		if (newSize_ > oldSize_) { \
			darray_growalloc(arr, newSize_); \
			memset(&(arr).item[oldSize_], 0, (newSize_ - oldSize_) * sizeof(*(arr).item)); \
		} \
	} while(0)

#define darray_realloc(arr, newAlloc) do { \
		(arr).item = realloc((arr).item, ((arr).alloc = (newAlloc)) * sizeof(*(arr).item)); \
	} while(0)
#define darray_growalloc(arr, need) do { \
		size_t need_ = (need); \
		if (need_ > (arr).alloc) \
			darray_realloc(arr, darray_next_alloc((arr).alloc, need_)); \
	} while(0)

#if HAVE_STATEMENT_EXPR==1
#define darray_make_room(arr, room) ({size_t newAlloc = (arr).size+(room); if ((arr).alloc<newAlloc) darray_realloc(arr, newAlloc); (arr).item+(arr).size; })
#endif

static inline size_t darray_next_alloc(size_t alloc, size_t need)
{
	if (alloc == 0)
		alloc = 1;
	while (alloc < need)
		alloc *= 2;
	return alloc;
}


/*** Traversal ***/

/*
 * darray_foreach(T *&i, darray(T) arr) {...}
 *
 * Traverse a darray.  `i` must be declared in advance as a pointer to an item.
 */
#define darray_foreach(i, arr) \
	for ((i) = &(arr).item[0]; (i) < &(arr).item[(arr).size]; (i)++)

/*
 * darray_foreach_reverse(T *&i, darray(T) arr) {...}
 *
 * Like darray_foreach, but traverse in reverse order.
 */
#define darray_foreach_reverse(i, arr) \
	for ((i) = &(arr).item[(arr).size]; (i)-- > &(arr).item[0]; )


#endif /* CCAN_DARRAY_H */

/*

darray_growalloc(arr, newAlloc) sees if the darray can currently hold newAlloc items;
	if not, it increases the alloc to satisfy this requirement, allocating slack
	space to avoid having to reallocate for every size increment.

darray_from_string(arr, str) copies a string to an darray_char.

darray_push(arr, item) pushes an item to the end of the darray.
darray_pop(arr) pops it back out.  Be sure there is at least one item in the darray before calling.
darray_pop_check(arr) does the same as darray_pop, but returns NULL if there are no more items left in the darray.

darray_make_room(arr, room) ensures there's 'room' elements of space after the end of the darray, and it returns a pointer to this space.
Currently requires HAVE_STATEMENT_EXPR, but I plan to remove this dependency by creating an inline function.

The following require HAVE_TYPEOF==1 :

darray_appends(arr, item0, item1...) appends a collection of comma-delimited items to the darray.
darray_prepends(arr, item0, item1...) prepends a collection of comma-delimited items to the darray.\


Examples:

	darray(int)  arr;
	int        *i;
	
	darray_appends(arr, 0,1,2,3,4);
	darray_appends(arr, -5,-4,-3,-2,-1);
	darray_foreach(i, arr)
		printf("%d ", *i);
	printf("\n");
	
	darray_free(arr);
	

	typedef struct {int n,d;} Fraction;
	darray(Fraction) fractions;
	Fraction        *i;
	
	darray_appends(fractions, {3,4}, {3,5}, {2,1});
	darray_foreach(i, fractions)
		printf("%d/%d\n", i->n, i->d);
	
	darray_free(fractions);
*/
