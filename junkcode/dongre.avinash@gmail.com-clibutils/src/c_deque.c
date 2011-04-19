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

#include "c_lib.h"

#include <string.h>

#define CLIB_DEQUE_INDEX(x)  ((char *)(pDeq)->pElements + (sizeof(struct clib_object) * (x)))

static clib_error 
insert_clib_deque ( struct clib_deque *pDeq, int index, void *elem,size_t elem_size) {
    clib_error rc           = CLIB_ERROR_SUCCESS;
    struct clib_object* pObject = new_clib_object ( elem, elem_size );
    if ( ! pObject )
        return CLIB_ARRAY_INSERT_FAILED;

    pDeq->pElements[index] = pObject;
    pDeq->no_of_elements++;
    return rc;
}

static struct clib_deque*
grow_deque ( struct clib_deque *pDeq ) {

   	pDeq->no_max_elements = pDeq->no_max_elements * 2;
    pDeq->pElements        = (struct clib_object**) realloc ( pDeq->pElements, 
                                 pDeq->no_max_elements * sizeof ( struct clib_object*));
    return pDeq;

}
struct clib_deque*  
new_clib_deque( int deq_size , clib_compare fn_c, clib_destroy fn_d) {

    struct clib_deque *pDeq = (struct clib_deque*)malloc(sizeof(struct clib_deque));
	if ( pDeq == (struct clib_deque*)0 )
		return (struct clib_deque*)0;

    pDeq->no_max_elements  = deq_size < 8 ? 8 : deq_size;
    pDeq->pElements = (struct clib_object**) malloc(pDeq->no_max_elements * sizeof(struct clib_object*));

	if ( pDeq == (struct clib_deque*)0 )
		return (struct clib_deque*)0;

    pDeq->compare_fn      = fn_c;
    pDeq->destruct_fn     = fn_d;
    pDeq->head            = (int)pDeq->no_max_elements / 2;
    pDeq->tail            = pDeq->head + 1;
    pDeq->no_of_elements  = 0;    

    return pDeq;
}
clib_error 
push_back_clib_deque(struct clib_deque *pDeq, void *elem, size_t elem_size) {
	if ( pDeq == (struct clib_deque*)0 )
		return CLIB_DEQUE_NOT_INITIALIZED;

    if ( pDeq->tail == pDeq->no_max_elements )
        pDeq = grow_deque(pDeq);

    insert_clib_deque(pDeq, pDeq->tail, elem, elem_size);
    pDeq->tail++;

    return CLIB_ERROR_SUCCESS;
}
clib_error 
push_front_clib_deque(struct clib_deque *pDeq, void *elem,size_t elem_size) {
    clib_error rc = CLIB_ERROR_SUCCESS;	
    int to        = 0;
    int from      = 0;
    int count     = 0;

    if ( pDeq->head == 0 ) {
        pDeq = grow_deque(pDeq);
        to    = (pDeq->no_max_elements - pDeq->no_of_elements)/2;
        from  = pDeq->head + 1;
        count = pDeq->tail - from + 1;
        memmove (&(pDeq->pElements[to]), &(pDeq->pElements[from]), count * sizeof (struct clib_object*));
        pDeq->head = to - 1;
        pDeq->tail  = pDeq->head + count;
    }
    insert_clib_deque(pDeq, pDeq->head, elem, elem_size);
    pDeq->head--;
    return rc;
}

clib_error     
front_clib_deque (struct clib_deque *pDeq, void *elem) {
	if ( pDeq == (struct clib_deque*)0 )
		return CLIB_DEQUE_NOT_INITIALIZED;
    element_at_clib_deque ( pDeq, pDeq->head + 1, elem );
    return CLIB_ERROR_SUCCESS;
}

clib_error 
back_clib_deque (struct clib_deque *pDeq, void *elem) {
	if ( pDeq == (struct clib_deque*)0 )
		return CLIB_DEQUE_NOT_INITIALIZED;
    element_at_clib_deque ( pDeq, pDeq->tail - 1, elem );
    return CLIB_ERROR_SUCCESS;
}

clib_error     
pop_back_clib_deque (struct clib_deque *pDeq) {
	if ( pDeq == (struct clib_deque*)0 )
		return CLIB_DEQUE_NOT_INITIALIZED;

    if ( pDeq->destruct_fn ) {
        void *elem;
        if ( element_at_clib_deque( pDeq, pDeq->tail - 1, &elem ) == CLIB_ERROR_SUCCESS ) 
            pDeq->destruct_fn(elem);
    }
    delete_clib_object(pDeq->pElements[pDeq->tail - 1]);
    pDeq->tail--;
    pDeq->no_of_elements--;

    return CLIB_ERROR_SUCCESS;

}

clib_error     
pop_front_clib_deque(struct clib_deque *pDeq) {
    
	if ( pDeq == (struct clib_deque*)0 )
		return CLIB_DEQUE_NOT_INITIALIZED;

    if ( pDeq->destruct_fn ) {
        void *elem;
        if ( element_at_clib_deque( pDeq, pDeq->head + 1, &elem ) == CLIB_ERROR_SUCCESS ) 
            pDeq->destruct_fn(elem);
    }
    delete_clib_object(pDeq->pElements[pDeq->head + 1]);

    pDeq->head++;
    pDeq->no_of_elements--;

    return CLIB_ERROR_SUCCESS;
}
clib_bool      
empty_clib_deque (struct clib_deque *pDeq) {
	if ( pDeq == (struct clib_deque*)0 )
		return clib_true;

    return pDeq->no_of_elements == 0 ? clib_true : clib_false;
}
int 
size_clib_deque( struct clib_deque *pDeq ) {
	if ( pDeq == (struct clib_deque*)0 )
		return clib_true;

    return pDeq->no_of_elements - 1;
}

clib_error 
element_at_clib_deque (struct clib_deque *pDeq, int index, void**elem) {

    clib_error rc = CLIB_ERROR_SUCCESS;

    if ( ! pDeq )
        return CLIB_DEQUE_NOT_INITIALIZED;

    get_raw_clib_object ( pDeq->pElements[index], elem );
    return rc;
}

clib_error
delete_clib_deque ( struct clib_deque *pDeq ) {
    int i = 0;

	if ( pDeq == (struct clib_deque*)0 )
		return CLIB_ERROR_SUCCESS;

    if ( pDeq->destruct_fn ) {
        for ( i = pDeq->head + 1; i < pDeq->tail; i++ ){
            void *elem;
            if ( element_at_clib_deque( pDeq, i, &elem ) == CLIB_ERROR_SUCCESS ) {
                pDeq->destruct_fn(elem);
            }
        }
    }
    for ( i = pDeq->head + 1; i < pDeq->tail; i++ ){
        delete_clib_object(pDeq->pElements[i]);
    }
    free ( pDeq->pElements);
    free ( pDeq );

    return CLIB_ERROR_SUCCESS;
}
void 
for_each_clib_deque ( struct clib_deque *pDeq, void (*fn)(void*)) {
	int index = 0;
	for ( index = pDeq->head + 1; index < pDeq->tail; index++ ){
		void *elem;
		if ( element_at_clib_deque( pDeq, index, &elem ) == CLIB_ERROR_SUCCESS ) {
			(fn)(elem);
			free ( elem );
		}
	}
}
