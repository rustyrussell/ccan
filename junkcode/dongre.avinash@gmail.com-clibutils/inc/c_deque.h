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

#ifndef _C_DEQUE_H_
#define _C_DEQUE_H_

struct clib_deque {
    struct clib_object**pElements;
    int no_max_elements;
    int head;
    int tail;
    int no_of_elements;
    clib_compare compare_fn;
    clib_destroy destruct_fn;
}c_deque;

extern struct clib_deque* new_clib_deque( int deq_size , clib_compare fn_c, clib_destroy fn_d);
extern clib_error     push_back_clib_deque (struct clib_deque *pDeq, void *elem, size_t elem_size);
extern clib_error     push_front_clib_deque(struct clib_deque *pDeq, void *elem,size_t elem_size);

extern clib_error     front_clib_deque     (struct clib_deque *pDeq,void*);
extern clib_error     back_clib_deque      (struct clib_deque *pDeq,void*);
extern clib_error     pop_back_clib_deque  (struct clib_deque *pDeq);
extern clib_error     pop_front_clib_deque (struct clib_deque *pDeq);
extern clib_bool      empty_clib_deque     (struct clib_deque *pDeq);
extern int            size_clib_deque ( struct clib_deque *pDeq);
extern clib_error     delete_clib_deque ( struct clib_deque *pDeq);
extern clib_error     element_at_clib_deque (struct clib_deque *pDeq, int index, void**elem);
extern void for_each_clib_deque ( struct clib_deque *pDeq, void (*fn)(void*));

#endif
