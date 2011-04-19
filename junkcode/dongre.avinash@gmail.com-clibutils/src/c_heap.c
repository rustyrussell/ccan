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

static int
pvt_clib_heap_isLeaf( int pos , struct clib_heap *pHeap) { 
	return ( pos >= (pHeap->pHeapPtr->no_of_elements/2)) && ( pos < pHeap->pHeapPtr->no_of_elements );
}

static int 
pvt_clib_heap_leftchild( int pos ) {
      return 2 * pos + 1;            
}

static int
pvt_clib_heap_compare(struct clib_array *pArray, int lIndex , int rIndex) {
	void *left         =  (void*)0;
	void *right        = (void*)0;
	int compare_result = 0;
	clib_error rc      = 0;
	rc = element_at_clib_array ( pArray, lIndex , &left);
	rc = element_at_clib_array ( pArray, rIndex , &right);
	compare_result =  pArray->compare_fn ( left, right );
	if ( left ) free ( left );
	if ( right ) free ( right );
	return compare_result;
}

static void
pvt_clib_heap_siftdown_max( struct clib_heap *pHeap, int pos ) {

	struct clib_array *pArray = pHeap->pHeapPtr;
	int n = pArray->no_of_elements;

	while ( !pvt_clib_heap_isLeaf(pos, pHeap) ) {
		int j = pvt_clib_heap_leftchild( pos );
		if ( ( j < ( n - 1) )  &&  
			(pvt_clib_heap_compare( pArray, j, j+1) == -1)) {
			j++;
		}
		if ( pvt_clib_heap_compare( pArray, pos, j ) == 1 || 
			 pvt_clib_heap_compare( pArray, pos, j ) == 0) return;

		swap_element_clib_array(pArray, pos, j);
		pos = j;
	}
}

static void
pvt_clib_heap_siftdown_min( struct clib_heap *pHeap, int pos ) {

	struct clib_array *pArray = pHeap->pHeapPtr;
	int n = pArray->no_of_elements;

	while ( !pvt_clib_heap_isLeaf(pos, pHeap) ) {
		int j = pvt_clib_heap_leftchild( pos );
		if ( ( j < ( n - 1) )  &&  
			(pvt_clib_heap_compare( pArray, j, j+1) == 1)) {
			j++;
		}
		if ( pvt_clib_heap_compare( pArray, pos, j ) == -1 || 
			 pvt_clib_heap_compare( pArray, pos, j ) == 0) return;

		swap_element_clib_array(pArray, pos, j);
		pos = j;
	}
}

struct clib_heap *
new_clib_heap( int default_size, clib_compare fn_c, clib_destroy fn_d ) {
	struct clib_heap *pHeap = ( struct clib_heap *) malloc ( sizeof ( struct clib_heap ));
	pHeap->pHeapPtr =  new_clib_array ( default_size, fn_c, fn_d);
	pHeap->heap_left = 0;
	pHeap->heap_parent = 0;
	pHeap->heap_right = 0;
	return pHeap;
}

void 
delete_clib_heap( struct clib_heap *pHeap) {
	delete_clib_array ( pHeap->pHeapPtr );
	free ( pHeap );
}

void 
insert_clib_heap ( struct clib_heap *pHeap, void *elem, size_t elem_size) {
	push_back_clib_array ( pHeap->pHeapPtr, elem, elem_size);
}

void
build_max_clib_heap ( struct clib_heap *pHeap ) {
	int i = 0;
	for (  i = (pHeap->pHeapPtr->no_of_elements / 2 ) - 1; i >= 0; i--) {
		pvt_clib_heap_siftdown_max(pHeap, i);
	}
}
void *
extract_max_clib_heap( struct clib_heap *pHeap) {
	void *elem;
	swap_element_clib_array(pHeap->pHeapPtr, 
		                    0,
							pHeap->pHeapPtr->no_of_elements - 1);

	back_clib_array( pHeap->pHeapPtr, &elem);
	remove_clib_array ( pHeap->pHeapPtr, pHeap->pHeapPtr->no_of_elements - 1 );

	if (pHeap->pHeapPtr->no_of_elements != 0) {
		pvt_clib_heap_siftdown_max(pHeap, 0);
	}

	return elem;
}

void
build_min_clib_heap ( struct clib_heap *pHeap ) {
	int i = 0;
	for (  i = (pHeap->pHeapPtr->no_of_elements / 2 ) - 1; i >= 0; i--) {
		pvt_clib_heap_siftdown_min(pHeap, i);
	}
}

void *
extract_min_clib_heap( struct clib_heap *pHeap) {
	void *elem;
	swap_element_clib_array(pHeap->pHeapPtr, 
		                    0,
							pHeap->pHeapPtr->no_of_elements - 1);

	back_clib_array( pHeap->pHeapPtr, &elem);
	remove_clib_array ( pHeap->pHeapPtr, pHeap->pHeapPtr->no_of_elements - 1 );

	if (pHeap->pHeapPtr->no_of_elements != 0) {
		pvt_clib_heap_siftdown_min(pHeap, 0);
	}
	return elem;
}
clib_bool      
empty_clib_heap ( struct clib_heap *pHeap) {
	if ( pHeap == ( struct clib_heap*)0 )
		return clib_true;

	return pHeap->pHeapPtr->no_of_elements == 0 ? clib_true : clib_false;
}


void for_each_clib_heap ( struct clib_heap *pHeap, void (*fn)(void*)) {
	int size = size_clib_array ( pHeap->pHeapPtr );
	int i = 0;
	for ( i = 0; i < size; i++ ) {
		void *elem;
		element_at_clib_array ( pHeap->pHeapPtr, i , &elem);
		(fn)(elem);
		free ( elem );
	}
}
