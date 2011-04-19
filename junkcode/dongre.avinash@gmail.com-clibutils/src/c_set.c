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

struct clib_set* 
new_clib_set ( clib_compare fn_c, clib_destroy fn_d) {

    struct clib_set *pSet  =  (struct clib_set*)malloc(sizeof(struct clib_set));
    if (pSet == (struct clib_set*)0 )
        return (struct clib_set*)0 ;

    pSet->root  = new_clib_rb (fn_c, fn_d, (void*)0);
    if (pSet->root == (struct clib_rb*)0)
        return (struct clib_set*)0 ;

    return pSet;
}
clib_error   
insert_clib_set (struct clib_set *pSet, void *key, size_t key_size) {
    if (pSet == (struct clib_set*)0 )
        return CLIB_SET_NOT_INITIALIZED;

    return insert_clib_rb ( pSet->root, key, key_size, (void*)0, 0);
}
clib_bool    
exists_clib_set ( struct clib_set *pSet, void *key) {
    clib_bool found = clib_false;
    struct clib_rb_node* node;

    if (pSet == (struct clib_set*)0 )
        return clib_false;
    
    node = find_clib_rb ( pSet->root, key);
    if ( node != (struct clib_rb_node*)0  ) {
        return clib_true;
    }
    return found;    
}
clib_error   
remove_clib_set ( struct clib_set *pSet, void *key) {
    clib_error rc = CLIB_ERROR_SUCCESS;
    struct clib_rb_node* node;
    if (pSet == (struct clib_set*)0 )
        return CLIB_SET_NOT_INITIALIZED;

    node = remove_clib_rb ( pSet->root, key );
    if ( node != (struct clib_rb_node*)0  ) {
        /*free ( node->raw_data.key);
        free ( node );*/
    }
    return rc;
}
clib_bool    
find_clib_set ( struct clib_set *pSet, void *key, void* outKey) {
    struct clib_rb_node* node;

    if (pSet == (struct clib_set*)0 )
        return clib_false;

    node = find_clib_rb ( pSet->root, key);
    if ( node == (struct clib_rb_node*)0  ) 
        return clib_false;

    get_raw_clib_object ( node->key, outKey );

    return clib_true;

}

clib_error    
delete_clib_set ( struct clib_set* x) {
    clib_error rc = CLIB_ERROR_SUCCESS;
    if ( x != (struct clib_set*)0  ){
        rc = delete_clib_rb ( x->root );
        free ( x );
    }
    return rc;
}
static struct clib_rb_node *
minimum_clib_set( struct clib_set *x ) {
	return minimum_clib_rb( x->root, x->root->root);
}

