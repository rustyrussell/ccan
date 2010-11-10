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

#ifndef CCAN_ARRAY_H
#define CCAN_ARRAY_H

#define ARRAY_USE_TALLOC

#include <stdlib.h>
#include <string.h>
#include "config.h"

#ifdef ARRAY_USE_TALLOC
#include <ccan/talloc/talloc.h>
#endif

//Use the array_alias macro to indicate that a pointer has changed but strict aliasing rules are too stupid to know it
#if HAVE_ATTRIBUTE_MAY_ALIAS
#define array_alias(ptr) /* nothing */
#define array(type) struct {type *item; size_t size; size_t alloc;} __attribute__((__may_alias__))
#else
#define array_alias(ptr) qsort(ptr, 0, 1, array_alias_helper) //hack
#define array(type) struct {type *item; size_t size; size_t alloc;}
#endif

//We call allocator functions directly
#ifndef ARRAY_USE_TALLOC

#define array_new() {0,0,0}
#define array_init(array) do {(array).item=0; (array).size=0; (array).alloc=0;} while(0)
#define array_realloc(array, newAlloc) do {(array).item = realloc((array).item, ((array).alloc = (newAlloc))*sizeof(*(array).item));} while(0)
#define array_free(array) do {free((array).item);} while(0)

#else

//note:  the allocations are given an extra byte to prevent free (and thus loss of ctx) on realloc to size 0

#define array_new(ctx) {talloc_size(ctx,1), 0,0}
#define array_init(array, ctx) do {(array).item=talloc_size(ctx,1); (array).size=0; (array).alloc=0;} while(0)
#define array_realloc(array, newAlloc) do {(array).item = talloc_realloc_size(NULL, (array).item, ((array).alloc = (newAlloc))*sizeof(*(array).item) +1);} while(0)
#define array_free(array) do {talloc_free((array).item);} while(0)

#endif


//We call helper functions
#define array_resize(array, newSize) do {(array).size = (newSize); if ((array).size > (array).alloc) { array_resize_helper((array_char*)&(array), sizeof(*(array).item)); array_alias(&(array));}} while(0)
#define array_resize0(array, newSize) do {array_resize0_helper((array_char*)&(array), sizeof(*(array).item), newSize);} while(0)
#define array_prepend_lit(array, stringLiteral) do {array_insert_items_helper((array_char*)&(array), sizeof(*(array).item), 0, stringLiteral, sizeof(stringLiteral)-1, 1); array_alias(&(array)); (array).item[--(array).size] = 0;} while(0)
#define array_prepend_string(array, str) do {const char *__str = (str); size_t __len = strlen(__str); array_insert_items_helper((array_char*)&(array), sizeof(*(array).item), 0, __str, __len, 1); array_alias(&(array)); (array).item[--(array).size] = 0;} while(0)
#define array_prepend_items(array, items, count) do {array_insert_items_helper((array_char*)&(array), sizeof(*(array).item), 0, items, count, 0); array_alias(&(array));} while(0)


//We call other array_* macros
#define array_from_c(array, c_array) array_from_items(array, c_array, sizeof(c_array)/sizeof(*(c_array)))
#define array_from_lit(array, stringLiteral) do {array_from_items(array, stringLiteral, sizeof(stringLiteral)); (array).size--;} while(0)
#define array_from_string(array, str) do {const char *__str = (str); array_from_items(array, __str, strlen(__str)+1); (array).size--;} while(0)
#define array_from_items(array, items, count) do {size_t __count = (count); array_resize(array, __count); memcpy((array).item, items, __count*sizeof(*(array).item));} while(0)
#define array_append(array, ...) do {array_resize(array, (array).size+1); (array).item[(array).size-1] = (__VA_ARGS__);} while(0)
#define array_append_string(array, str) do {const char *__str = (str); array_append_items(array, __str, strlen(__str)+1); (array).size--;} while(0)
#define array_append_lit(array, stringLiteral) do {array_append_items(array, stringLiteral, sizeof(stringLiteral)); (array).size--;} while(0)
#define array_append_items(array, items, count) do {size_t __count = (count); array_resize(array, (array).size+__count); memcpy((array).item+(array).size-__count, items, __count*sizeof(*(array).item));} while(0)
#define array_prepend(array, ...) do {array_resize(array, (array).size+1); memmove((array).item+1, (array).item, ((array).size-1)*sizeof(*(array).item)); *(array).item = (__VA_ARGS__);} while(0)
#define array_push(array, ...) array_append(array, __VA_ARGS__)
#define array_pop_check(array) ((array).size ? array_pop(array) : NULL)

#define array_growalloc(array, newAlloc) do {size_t __newAlloc=(newAlloc); if (__newAlloc > (array).alloc) array_realloc(array, (__newAlloc+63)&~63); } while(0)
#if HAVE_STATEMENT_EXPR==1
#define array_make_room(array, room) ({size_t newAlloc = (array).size+(room); if ((array).alloc<newAlloc) array_realloc(array, newAlloc); (array).item+(array).size; })
#endif


//We do just fine by ourselves
#define array_pop(array) ((array).item[--(array).size])

#define array_for_t(var, array, type, ...) do {type *var=(void*)(array).item; size_t _r=(array).size, _i=0; for (;_r--;_i++, var++) { __VA_ARGS__ ;} } while(0)

#define array_appends_t(array, type, ...) do {type __src[] = {__VA_ARGS__}; array_append_items(array, __src, sizeof(__src)/sizeof(*__src));} while(0)

#if HAVE_TYPEOF==1
#define array_appends(array, ...) array_appends_t(array, typeof((*(array).item)), __VA_ARGS__))
#define array_prepends(array, ...) do {typeof((*(array).item)) __src[] = {__VA_ARGS__}; array_prepend_items(array, __src, sizeof(__src)/sizeof(*__src));} while(0)
#define array_for(var, array, ...) array_for_t(var, array, typeof(*(array).item), __VA_ARGS__)
#define array_rof(var, array, ...) do {typeof(*(array).item) *var=(void*)(array).item; size_t _i=(array).size, _r=0; var += _i; for (;_i--;_r++) { var--; __VA_ARGS__ ;} } while(0)
#endif


typedef array(char) array_char;

void array_resize_helper(array_char *a, size_t itemSize);
void array_resize0_helper(array_char *a, size_t itemSize, size_t newSize);
void array_insert_items_helper(array_char *a, size_t itemSize, size_t pos, const void *items, size_t count, size_t tailSize);
	//Note:  there is no array_insert_items yet, but it wouldn't be too hard to add.

int array_alias_helper(const void *a, const void *b);

#endif

/*

array_growalloc(array, newAlloc) sees if the array can currently hold newAlloc items;
	if not, it increases the alloc to satisfy this requirement, allocating slack
	space to avoid having to reallocate for every size increment.

array_from_string(array, str) copys a string to an array_char.

array_push(array, item) pushes an item to the end of the array.
array_pop_nocheck(array) pops it back out.  Be sure there is at least one item in the array before calling.
array_pop(array) does the same as array_pop_nocheck, but returns NULL if there are no more items left in the array.

array_make_room(array, room) ensures there's 'room' elements of space after the end of the array, and it returns a pointer to this space.
Currently requires HAVE_STATEMENT_EXPR, but I plan to remove this dependency by creating an inline function.

The following require HAVE_TYPEOF==1 :

array_appends(array, item0, item1...) appends a collection of comma-delimited items to the array.
array_prepends(array, item0, item1...) prepends a collection of comma-delimited items to the array.
array_for(var, array, commands...) iterates forward through the items in the array using var.  var is a pointer to an item.
array_rof(var, array, commands...) iterates backward through the items in the array using var.  var is a pointer to an item.

Examples:

array(int) array;
array_appends(array, 0,1,2,3,4);
array_appends(array, -5,-4,-3,-2,-1);
array_for(i, array, printf("%d ", *i));
printf("\n");
array_free(array);

array(struct {int n,d;}) fractions;
array_appends(fractions, {3,4}, {3,5}, {2,1});
array_for(i, fractions, printf("%d/%d\n", i->n, i->d));
array_free(fractions);
*/

/*

Direct tests:

array_push
array_prepend
array_from_c
array_for
array_rof
array_from_lit
array_append_lit
array_prepend_lit
array_append_string
array_prepend_string
array_from_string
array_resize0
array_pop_nocheck
array_realloc
array_growalloc
array_make_room
array_pop
array_appends
array_prepends

Indirect tests:

array_append <- array_push
array_resize <- array_append
array_from_items <- array_from_c, array_from_lit, array_from_string
array_append_items <- array_append_string, array_append_lit
array_prepend_items <- array_prepends

Untested:

*/
