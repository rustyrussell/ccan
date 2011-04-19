/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **
 *  This file is part of clib library
 *  Copyright (C) 2011 Avinash Dongre ( dongre.avinash@gmail.com )
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 * 
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/
#ifndef _C_HEAP_H
#define _C_HEAP_H

struct clib_heap {
	struct clib_array *pHeapPtr;
	int heap_parent;
	int heap_left;
	int heap_right;
};

extern struct clib_heap *new_clib_heap( int default_size, clib_compare fn_c, clib_destroy fn_d );
extern void delete_clib_heap( struct clib_heap *pHeap);
extern void insert_clib_heap ( struct clib_heap *pHeap, void *elem, size_t elem_size);
extern void build_max_clib_heap ( struct clib_heap *pHeap );\
extern void build_min_clib_heap ( struct clib_heap *pHeap );
extern void *extract_max_clib_heap( struct clib_heap *pHeap);
extern void *extract_min_clib_heap( struct clib_heap *pHeap);
extern clib_bool empty_clib_heap( struct clib_heap *pHeap);
extern void for_each_clib_heap ( struct clib_heap *pHeap, void (*fn)(void*));

#endif