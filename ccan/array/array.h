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

#include <stdlib.h>
#include <string.h>
#include "config.h"
 
#define Array(type) struct {type *item; size_t size; size_t allocSize;}
#define NewArray() {0,0,0}
#define AInit(array) do {(array).item=0; (array).size=0; (array).allocSize=0;} while(0)
#define AFree(array) do {free((array).item);} while(0)
#define AResize(array, newSize) do {size_t __newSize=(newSize); if (__newSize > (array).allocSize) {(array).allocSize = (__newSize+63)&~63; (array).item = realloc((array).item, (array).allocSize*sizeof(*(array).item));} (array).size = __newSize; } while(0)
#define AResize0(array, newSize) do {size_t __oldSize=(array).size, __newSize=(newSize); if (__newSize <= __oldSize) (array).size = __newSize; else {AResize(array,newSize); memset((array).item+__oldSize,0,(__newSize-__oldSize)*sizeof(*(array).item));} } while(0)
#define ASetAllocSize(array, newAlloc) do {(array).item = realloc((array).item, ((array).allocSize = (newAlloc))*sizeof(*(array).item));} while(0)
#define AFromC(array, c_array) AFromItems(array, c_array, sizeof(c_array)/sizeof(*(c_array)))
#define AFromLit(array, stringLiteral) do {AFromItems(array, stringLiteral, sizeof(stringLiteral)); (array).size--;} while(0)
#define AFromString(array, str) do {const char *__str = (str); AFromItems(array, __str, strlen(__str)+1); (array).size--;} while(0)
#define AFromItems(array, items, count) do {size_t __count = (count); AResize(array, __count); memcpy((array).item, items, __count*sizeof(*(array).item));} while(0)
#define AAppend(array, newItem...) do {AResize(array, (array).size+1); (array).item[(array).size-1] = (newItem);} while(0)
#define AAppendString(array, str) do {const char *__str = (str); AAppendItems(array, __str, strlen(__str)+1); (array).size--;} while(0)
#define AAppendLit(array, stringLiteral) do {AAppendItems(array, stringLiteral, sizeof(stringLiteral)); (array).size--;} while(0)
#define AAppendItems(array, items, count) do {size_t __count = (count); AResize(array, (array).size+__count); memcpy((array).item+(array).size-__count, items, __count*sizeof(*(array).item));} while(0)
#define APrepend(array, newItem...) do {AResize(array, (array).size+1); memmove((array).item+1, (array).item, ((array).size-1)*sizeof(*(array).item)); *(array).item = (newItem);} while(0)
#define APrependItems(array, items, count) do {AResize(array, (array).size+(count)); memmove((array).item+(count), (array).item, ((array).size-(count))*sizeof(*(array).item)); memcpy((array).item, items, (count)*sizeof(*(array).item)); } while(0)
#define APush(array, value...) AAppend(array, value)
#define APop(array) ((array).item[--(array).size])

#if HAVE_STATEMENT_EXPR==1
#define AMakeRoom(array, room) ({size_t newAlloc = (array).size+(room); if ((array).allocSize<newAlloc) (array).item = realloc((array).item, ((array).allocSize=newAlloc)*sizeof(*(array).item)); (array).item+(array).size; })
#endif

#if HAVE_TYPEOF==1
#define APopS(array) ((array).size ? APop(array) : (typeof(*(array).item))0)
#define AAppends(array, items...) do {typeof((*(array).item)) __src[] = {items}; AAppendItems(array, __src, sizeof(__src)/sizeof(*__src));} while(0)
#define APrepends(array, items...) do {typeof((*(array).item)) __src[] = {items}; APrependItems(array, __src, sizeof(__src)/sizeof(*__src));} while(0)
#define AFor(var, array, commands...) {typeof(*(array).item) *var=(void*)(array).item; size_t __d=(array).size; for (;__d--;var++) { commands ;} }
#define ARof(var, array, commands...) {typeof(*(array).item) *var=(void*)(array).item; size_t __i=(array).size; var += __i; while (__i--) { var--; commands ;} }
#endif

typedef Array(char) ArrayChar;

#endif
