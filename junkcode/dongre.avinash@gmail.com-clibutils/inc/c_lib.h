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

#ifndef _C_LIB_H_
#define _C_LIB_H_

#include "c_errors.h"
#include <stdlib.h>

/* ------------------------------------------------------------------------*/
/*       C O M M O N       D E F I N I T O N S                             */
/* ------------------------------------------------------------------------*/

typedef void (*clib_destroy)(void*);
typedef int  (*clib_compare)(void*,void*);
typedef void (*clib_traversal)( void*);

typedef int  clib_error;
typedef int  clib_bool;

#define clib_black           0
#define clib_red             1
#define clib_true            1
#define clib_false           0

/* ------------------------------------------------------------------------*/
/*                            P  A  I   R                                  */
/* ------------------------------------------------------------------------*/

struct clib_object {
    void* raw_data;
    size_t size;
};

#include "c_array.h"
#include "c_deque.h"
#include "c_rb.h"
#include "c_set.h"
#include "c_map.h"
#include "c_slist.h"
#include "c_map.h"
#include "c_stack.h"
#include "c_heap.h"

/* ------------------------------------------------------------------------*/
/*            H E L P E R       F U N C T I O N S                          */
/* ------------------------------------------------------------------------*/

extern void  clib_copy ( void *destination, void *source, size_t size );
extern void  clib_get  ( void *destination, void *source, size_t size);
extern char* clib_strdup ( char *ptr );

extern struct clib_object* new_clib_object (void *inObject, size_t obj_size);
extern clib_error get_raw_clib_object (struct clib_object *inObject, void**elem);
extern void  delete_clib_object  (struct clib_object* inObject );
extern void replace_raw_clib_object(struct clib_object* current_object,void *elem, size_t elem_size);

#endif
