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
#include <stdio.h>

static int 
compare_int ( void *left, void *right ) {
    int *l = (int*)left;
    int *r = (int*)right;

    if ( *l < *r ) 
        return -1;
    else if ( *l > *r ) 
        return 1;
    return 0;
}
static void
print_element ( void *ptr ) {
	printf ( "%d\n", *(int*)ptr);
}
void 
test_clib_heap_max() {
	int test[] = {4,1,3,2,16,9,10,14,8,7};
	int index  = 0;
	int size   = sizeof (test) /sizeof(test[0]);
	void *maxElem;
	struct clib_heap* pHeap = new_clib_heap ( 8, compare_int, NULL);

	for ( index = 0; index < size; index++ ) {
		int v = test[index];
		insert_clib_heap ( pHeap, &v, sizeof(int));			
	}
	build_max_clib_heap( pHeap);
	printf ( "---------------------------------\n");
	for_each_clib_heap ( pHeap, print_element);
	printf ( "---------------------------------\n");
	while ( empty_clib_heap(pHeap) != clib_true ) {
		maxElem  = extract_max_clib_heap ( pHeap );
		printf ( "MAX ELEMENT = %d\n", *(int*)maxElem);
		free ( maxElem );
	}
	delete_clib_heap ( pHeap );
}

void
test_clib_heap_min() {
	int test[] = {4,1,3,2,16,9,10,14,8,7};
	int index  = 0;
	int size   = sizeof (test) /sizeof(test[0]);
	void *maxElem;
	struct clib_heap* pHeap = new_clib_heap ( 8, compare_int, NULL);

	for ( index = 0; index < size; index++ ) {
		int v = test[index];
		insert_clib_heap ( pHeap, &v, sizeof(int));			
	}
	build_min_clib_heap( pHeap);
	printf ( "---------------------------------\n");
	for_each_clib_heap ( pHeap, print_element);
	printf ( "---------------------------------\n");
	while ( empty_clib_heap(pHeap) != clib_true ) {
		maxElem  = extract_min_clib_heap ( pHeap );
		printf ( "MIN ELEMENT = %d\n", *(int*)maxElem);
		free ( maxElem );
	}
	delete_clib_heap ( pHeap );

}

void 
test_clib_heap() {
	test_clib_heap_max();
	test_clib_heap_min();
}