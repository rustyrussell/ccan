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
#include <string.h>
#include <assert.h>

#define rb_sentinel &pTree->sentinel

static void debug_verify_properties(struct clib_rb*);
static void debug_verify_property_1(struct clib_rb*,struct clib_rb_node*);
static void debug_verify_property_2(struct clib_rb*,struct clib_rb_node*);
static int debug_node_color(struct clib_rb*,struct clib_rb_node* n);
static void debug_verify_property_4(struct clib_rb*,struct clib_rb_node*);
static void debug_verify_property_5(struct clib_rb*,struct clib_rb_node*);
static void debug_verify_property_5_helper(struct clib_rb*,struct clib_rb_node*,int,int*);


static void
pvt_left_rotate(struct clib_rb *pTree, struct clib_rb_node *x){
    struct clib_rb_node *y;
    y = x->right;
    x->right = y->left;
    if (y->left != rb_sentinel)
        y->left->parent = x;
    if (y != rb_sentinel)
        y->parent = x->parent;
    if (x->parent){
        if (x == x->parent->left)
            x->parent->left = y;
        else
            x->parent->right = y;
    }
    else
        pTree->root = y;
    y->left = x;
    if (x != rb_sentinel)
        x->parent = y;
}
static void
pvt_right_rotate(struct clib_rb *pTree, struct clib_rb_node *x) {
    struct clib_rb_node *y = x->left;
    x->left = y->right;
    if (y->right != rb_sentinel)
        y->right->parent = x;
    if (y != rb_sentinel)
        y->parent = x->parent;
    if (x->parent) {
        if (x == x->parent->right)
            x->parent->right = y;
        else
            x->parent->left = y;
    }
    else
        pTree->root = y;
    y->right = x;
    if (x != rb_sentinel)
        x->parent = y;
}

struct clib_rb*
new_clib_rb(clib_compare fn_c,clib_destroy fn_ed, clib_destroy fn_vd ){

    struct clib_rb *pTree = (struct clib_rb*)malloc(sizeof(struct clib_rb));
    if ( pTree == (struct clib_rb*)0 )
        return (struct clib_rb*)0;

    pTree->compare_fn           = fn_c;
    pTree->destruct_k_fn        = fn_ed;
    pTree->destruct_v_fn        = fn_vd;
    pTree->root                 = rb_sentinel;
    pTree->sentinel.left        = rb_sentinel;
    pTree->sentinel.right       = rb_sentinel;
    pTree->sentinel.parent      = (struct clib_rb_node*)0 ;
    pTree->sentinel.color       = clib_black;

    return pTree;
}
static void
pvt_rb_insert_fixup( struct clib_rb *pTree, struct clib_rb_node *x ) {
    while (x != pTree->root && x->parent->color == clib_red) {
        if (x->parent == x->parent->parent->left) {
            struct clib_rb_node *y = x->parent->parent->right;
            if (y->color == clib_red) {
                x->parent->color         = clib_black;
                y->color                 = clib_black;
                x->parent->parent->color = clib_red;
                x = x->parent->parent;
            } else {
                if (x == x->parent->right){
                    x = x->parent;
                    pvt_left_rotate (pTree, x);
                }
                x->parent->color         = clib_black;
                x->parent->parent->color = clib_red;
                pvt_right_rotate (pTree, x->parent->parent);
            }
        } else {
            struct clib_rb_node *y = x->parent->parent->left;
            if (y->color == clib_red) {
                x->parent->color         = clib_black;
                y->color                 = clib_black;
                x->parent->parent->color = clib_red;
                x = x->parent->parent;
            } else {
                if (x == x->parent->left) {
                    x = x->parent;
                    pvt_right_rotate (pTree, x);
                }
                x->parent->color         = clib_black;
                x->parent->parent->color = clib_red;
                pvt_left_rotate (pTree, x->parent->parent);
            }
        }
    }
    pTree->root->color = clib_black;
}
struct clib_rb_node*   
find_clib_rb (struct clib_rb *pTree, void *key) {
    struct clib_rb_node *x = pTree->root;

    while (x != rb_sentinel) {
        int c = 0;
        void *cur_key ;
        get_raw_clib_object ( x->key, &cur_key );
        c = pTree->compare_fn (key, cur_key);
        free ( cur_key );
        if (c == 0) {
            break;
        } else {
            x = c < 0 ? x->left : x->right;
        }
    }
    if ( x == rb_sentinel )
        return (struct clib_rb_node*)0 ;

    return x;
}

clib_error  
insert_clib_rb(struct clib_rb *pTree, void *k, size_t key_size, void *v, size_t value_size) {

    clib_error rc = CLIB_ERROR_SUCCESS;
    struct clib_rb_node *x;
	struct clib_rb_node *y;
	struct clib_rb_node *z;

    x = (struct clib_rb_node*)malloc (sizeof(struct clib_rb_node));
    if ( x == (struct clib_rb_node*)0  ) 
        return CLIB_ERROR_MEMORY;

    x->left    = rb_sentinel;
    x->right   = rb_sentinel;
    x->color   = clib_red;

    x->key     = new_clib_object ( k, key_size );
    if ( v ) {
        x->value   = new_clib_object ( v, value_size );
    } else {
        x->value =  (struct clib_object*)0;
    }

    y = pTree->root;
    z = (struct clib_rb_node*)0 ;

    while (y != rb_sentinel) {
        int c = 0;
        void *cur_key;
		void* new_key;

        get_raw_clib_object ( y->key, &cur_key );
        get_raw_clib_object ( x->key, &new_key );

        c = (pTree->compare_fn) ( new_key , cur_key);
        free ( cur_key );
        free ( new_key );
        if (c == 0) {
            /* TODO : Delete node here */
            return CLIB_RBTREE_KEY_DUPLICATE;
        }
        z = y;
        if ( c < 0 )
            y = y->left;
        else
            y = y->right;
    }    
    x->parent = z;
    if (z) {
        int c = 0;
        void *cur_key;
		void* new_key;
        get_raw_clib_object ( z->key, &cur_key );
        get_raw_clib_object ( x->key, &new_key );

        c = pTree->compare_fn( new_key, cur_key);
        free ( cur_key );
        free ( new_key );
        if (c < 0) {
            z->left = x;
        } else {
            z->right = x;
        }
    }
    else
        pTree->root = x;

    pvt_rb_insert_fixup (pTree, x);

    debug_verify_properties ( pTree);
    return rc;
}
static void
pvt_rb_remove_fixup( struct clib_rb *pTree, struct clib_rb_node *x ) {
    while (x != pTree->root && x->color == clib_black) {
        if (x == x->parent->left) {
            struct clib_rb_node *w = x->parent->right;
            if (w->color == clib_red) {
                w->color         = clib_black;
                x->parent->color = clib_red;
                pvt_left_rotate (pTree, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == clib_black && w->right->color == clib_black) {
                w->color = clib_red;
                x = x->parent;
            } else {
                if (w->right->color == clib_black)  {
                    w->left->color = clib_black;
                    w->color       = clib_red;
                    pvt_right_rotate (pTree, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = clib_black;
                w->right->color = clib_black;
                pvt_left_rotate (pTree, x->parent);
                x = pTree->root;
            }
        } else {
            struct clib_rb_node *w = x->parent->left;
            if (w->color == clib_red) {
                w->color = clib_black;
                x->parent->color = clib_red;
                pvt_right_rotate (pTree, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == clib_black && w->left->color == clib_black) {
                w->color = clib_red;
                x = x->parent;
            } else {
                if (w->left->color == clib_black) {
                    w->right->color = clib_black;
                    w->color = clib_red;
                    pvt_left_rotate (pTree, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = clib_black;
                w->left->color = clib_black;
                pvt_right_rotate (pTree, x->parent);
                x = pTree->root;
            }
        }
    }
    x->color = clib_black;
}

static struct clib_rb_node*  
pvt_remove_clib_rb(struct clib_rb *pTree, struct clib_rb_node *z ) {
    struct clib_rb_node *x = (struct clib_rb_node*)0 ;
    struct clib_rb_node *y = (struct clib_rb_node*)0 ;

    if (z->left == rb_sentinel || z->right == rb_sentinel)
        y = z;
    else {
        y = z->right;
        while (y->left != rb_sentinel)
            y = y->left;
    }
    if (y->left != rb_sentinel)
        x = y->left;
    else
        x = y->right;

    x->parent = y->parent;
    if (y->parent)
    {
        if (y == y->parent->left)
            y->parent->left = x;
        else
            y->parent->right = x;
    }
    else
        pTree->root = x;
    if (y != z) {
        struct clib_object* tmp;
        tmp    = z->key;
        z->key = y->key;
        y->key = tmp;

        tmp      = z->value;
        z->value = y->value;
        y->value = tmp;
    }
    if (y->color == clib_black)
        pvt_rb_remove_fixup (pTree, x);

    debug_verify_properties ( pTree);
    return y;
}

struct clib_rb_node*
remove_clib_rb (struct clib_rb *pTree, void *key) {
    struct clib_rb_node *z = (struct clib_rb_node*)0 ;

    z = pTree->root;
    while (z != rb_sentinel) {
        int c = 0;
        void *cur_key;
        get_raw_clib_object ( z->key, &cur_key );
        c = pTree->compare_fn (key, cur_key);
        free ( cur_key );
        if ( c == 0) {
            break;
        }
        else {
            z = ( c < 0) ? z->left : z->right;
        }
    }
    if (z == rb_sentinel)
        return (struct clib_rb_node*)0 ;
    return pvt_remove_clib_rb(pTree, z );
}
static void
pvt_delete_clib_rb_node (struct clib_rb *pTree, struct clib_rb_node *x ) {

    void *key;
	void *value;

    if ( pTree->destruct_k_fn ) {
        get_raw_clib_object ( x->key, &key );
        pTree->destruct_k_fn ( key );
    }
    delete_clib_object( x->key );

    if ( x->value ) {
        if ( pTree->destruct_v_fn ) {
            get_raw_clib_object ( x->value, &value);
            pTree->destruct_v_fn ( value );
        }
        delete_clib_object( x->value );
    }
}

clib_error  
delete_clib_rb(struct clib_rb *pTree) {

    clib_error rc = CLIB_ERROR_SUCCESS;
    struct clib_rb_node *z = pTree->root;

    while (z != rb_sentinel) {
        if (z->left != rb_sentinel)
            z = z->left;
        else if (z->right != rb_sentinel)
            z = z->right;
        else {
            pvt_delete_clib_rb_node ( pTree, z );
            if (z->parent) {
                z = z->parent;
                if (z->left != rb_sentinel){
                    free ( z->left );
                    z->left = rb_sentinel;
                }else if (z->right != rb_sentinel){
                    free ( z->right );
                    z->right = rb_sentinel;
                }
            } else {
                free ( z );
                z = rb_sentinel;
            }
        }
    }
    free ( pTree );
    return rc;
}
struct clib_rb_node *
minimum_clib_rb( struct clib_rb *pTree, struct clib_rb_node *x ) {
	while ( x->left != rb_sentinel)
		x = x->left;
	return x;
}

struct clib_rb_node *
maximum_clib_rb( struct clib_rb *pTree, struct clib_rb_node *x ) {
	while ( x->right != rb_sentinel)
		x = x->right;
	return x;
}


clib_bool 
empty_clib_rb(struct clib_rb *pTree) {
    if ( pTree->root != rb_sentinel )
        return clib_true;
    return clib_false;
}
struct clib_rb_node*
tree_successor(struct clib_rb *pTree, struct clib_rb_node *x) {
	struct clib_rb_node *y = (struct clib_rb_node*)0;
	if ( x->right != rb_sentinel)
		return minimum_clib_rb( pTree, x->right);
	
	if ( x  == maximum_clib_rb(pTree,pTree->root)) 
		return (struct clib_rb_node*)0;

	y = x->parent;
	while ( y != rb_sentinel && x == y->right ){
		x = y;
		y = y->parent;
	}
	return y;
}


void debug_verify_properties(struct clib_rb* t) {
    debug_verify_property_1(t,t->root);
    debug_verify_property_2(t,t->root);
    debug_verify_property_4(t,t->root);
    debug_verify_property_5(t,t->root);
}

void debug_verify_property_1(struct clib_rb *pTree,struct clib_rb_node* n) {
    assert(debug_node_color(pTree,n) == clib_red || debug_node_color(pTree,n) == clib_black);
    if (n == rb_sentinel) return;
    debug_verify_property_1(pTree,n->left);
    debug_verify_property_1(pTree,n->right);
}

void debug_verify_property_2(struct clib_rb *pTree,struct clib_rb_node* root) {
    assert(debug_node_color(pTree,root) == clib_black);
}

int debug_node_color(struct clib_rb *pTree,struct clib_rb_node* n) {
    return n == rb_sentinel ? clib_black : n->color;
}

void debug_verify_property_4(struct clib_rb *pTree,struct clib_rb_node* n) {
    if (debug_node_color(pTree,n) == clib_red) {
        assert (debug_node_color(pTree,n->left)   == clib_black);
        assert (debug_node_color(pTree,n->right)  == clib_black);
        assert (debug_node_color(pTree,n->parent) == clib_black);
    }
    if (n == rb_sentinel) return;
    debug_verify_property_4(pTree,n->left);
    debug_verify_property_4(pTree,n->right);
}

void debug_verify_property_5(struct clib_rb *pTree,struct clib_rb_node* root) {
    int black_count_path = -1;
    debug_verify_property_5_helper(pTree,root, 0, &black_count_path);
}

void debug_verify_property_5_helper(struct clib_rb *pTree,struct clib_rb_node* n, int black_count, int* path_black_count) {
    if (debug_node_color(pTree,n) == clib_black) {
        black_count++;
    }
    if (n == rb_sentinel) {
        if (*path_black_count == -1) {
            *path_black_count = black_count;
        } else {
            assert (black_count == *path_black_count);
        }
        return;
    }
    debug_verify_property_5_helper(pTree,n->left,  black_count, path_black_count);
    debug_verify_property_5_helper(pTree,n->right, black_count, path_black_count);
}

