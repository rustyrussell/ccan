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

#ifndef _C_ARRAY_H_
#define _C_ARRAY_H_

struct clib_array {
    int no_max_elements; /* Number of maximum elements array can hold without reallocation */
    int no_of_elements;  /* Number of current elements in the array */
    struct clib_object** pElements; /* actual storage area */
    clib_compare compare_fn; /* Compare function pointer*/
    clib_destroy destruct_fn; /* Destructor function pointer*/
};

extern struct clib_array* new_clib_array ( int init_size, clib_compare fn_c, clib_destroy fn_d);
extern clib_error push_back_clib_array ( struct clib_array *pArray, void *elem, size_t elem_size);
extern clib_error element_at_clib_array( struct clib_array *pArray, int pos, void **e);
extern clib_error insert_at_clib_array ( struct clib_array *pArray, int index, void *elem, size_t elem_size);
extern int size_clib_array( struct clib_array *pArray);
extern int capacity_clib_array( struct clib_array *pArray );
extern clib_bool  empty_clib_array( struct clib_array *pArray);
extern clib_error reserve_clib_array( struct clib_array *pArray, int pos);
extern clib_error front_clib_array( struct clib_array *pArray,void *elem);
extern clib_error back_clib_array( struct clib_array *pArray,void *elem);
extern clib_error remove_clib_array ( struct clib_array *pArray, int pos);
extern clib_error delete_clib_array( struct clib_array *pArray);
extern void swap_element_clib_array ( struct clib_array *pArray, int left, int right);
extern void for_each_clib_array ( struct clib_array *pArray, void (*fn)(void*));
#endif
