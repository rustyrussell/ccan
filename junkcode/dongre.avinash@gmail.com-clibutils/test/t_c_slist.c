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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

static void 
free_element ( void *ptr ) {
    if ( ptr )
        free ( ptr);
}

void
add_elements_to_list( struct clib_slist* ll, int x, int y ) {
    int i = 0;
    for ( i = x; i <= y; i++ ) { 
        int *v = ( int *) malloc ( sizeof ( int ));
        memcpy ( v, &i, sizeof ( int ));
        push_back_clib_slist ( ll, v , sizeof(v));
        free ( v );
    }
}
void
print_e ( void *ptr ) {
    if ( ptr )
        printf ( "%d\n", *(int*)ptr);
}

static int 
compare_element ( void *left, void *right ) {
    int *l = (int*) left;
    int *r = (int*) right;
    return *l == *r ;
}


void
test_clib_slist() {
    int i = 0;
    int *v;
    void* outValue;
    struct clib_slist* list = new_clib_slist(free_element,compare_element);

    add_elements_to_list(list,1, 10 );
    for_each_clib_slist(list, print_e);

    i = 55;
    v = ( int *) malloc ( sizeof ( int ));
    memcpy ( v, &i, sizeof ( int ));
    insert_clib_slist(list,5, v,sizeof(v));
    free ( v );
    for_each_clib_slist(list, print_e);

    remove_clib_slist(list,5);
    for_each_clib_slist(list, print_e);

    remove_clib_slist(list,0);
    for_each_clib_slist(list, print_e);

    remove_clib_slist(list,100);
    for_each_clib_slist(list, print_e);

    i = 1;
    v = ( int *) malloc ( sizeof ( int ));
    memcpy ( v, &i, sizeof ( int ));
    insert_clib_slist(list,1,v,sizeof(v));
    free ( v );
    for_each_clib_slist(list, print_e);

    i = 11;
    v = ( int *) malloc ( sizeof ( int ));
    memcpy ( v, &i, sizeof ( int ));
    insert_clib_slist(list,11,v,sizeof(v));
    free ( v );
    for_each_clib_slist(list, print_e);

    i = 12;
    v = ( int *) malloc ( sizeof ( int ));
    memcpy ( v, &i, sizeof ( int ));
    insert_clib_slist(list,200,v,sizeof(v));
    free ( v );
    for_each_clib_slist(list, print_e);

    remove_clib_slist(list,list->size);
    for_each_clib_slist(list, print_e);

    i = 10;
    if ( clib_true == find_clib_slist ( list, &i, &outValue)) {
        assert ( i == *(int*)outValue );
        free ( outValue );
    }
    i = 100;
    assert ( clib_false == find_clib_slist ( list, &i, &outValue));

    delete_clib_slist ( list );

}
