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

#ifndef _C_RB_H_
#define _C_RB_H_

struct clib_rb_node {
    struct clib_rb_node *left;
    struct clib_rb_node *right;
    struct clib_rb_node *parent;
    int color; 
    struct clib_object* key;
    struct clib_object* value; 
};

struct clib_rb {
    struct clib_rb_node* root;
    struct clib_rb_node sentinel;
    clib_destroy destruct_k_fn;
	clib_destroy destruct_v_fn;
    clib_compare compare_fn;
};

extern struct clib_rb* new_clib_rb(clib_compare fn_c,clib_destroy fn_ed, clib_destroy fn_vd );
extern clib_error  insert_clib_rb(struct clib_rb *pTree, void *key, size_t key_size, void *value, size_t value_size);
extern struct clib_rb_node*   find_clib_rb (struct clib_rb *pTree, void *key);
extern struct clib_rb_node* remove_clib_rb (struct clib_rb *pTree, void *key);
extern clib_error  delete_clib_rb (struct clib_rb *pTree);
extern clib_bool   empty_clib_rb  (struct clib_rb *pTree);

extern struct clib_rb_node *minimum_clib_rb( struct clib_rb *pTree, struct clib_rb_node *x );
extern struct clib_rb_node *maximum_clib_rb( struct clib_rb *pTree, struct clib_rb_node *x );

#endif