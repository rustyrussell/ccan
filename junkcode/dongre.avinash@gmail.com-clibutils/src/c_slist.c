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

struct clib_slist* 
new_clib_slist(clib_destroy fn_d, clib_compare fn_c){
    struct clib_slist *pSlist  = (struct clib_slist*)malloc(sizeof(struct clib_slist));
    pSlist->head           = (struct clib_slist_node*)0;
    pSlist->destruct_fn    = fn_d;
    pSlist->compare_fn = fn_c;
    pSlist->size           = 0;
    return pSlist;
}
void           
delete_clib_slist( struct clib_slist *pSlist){
    while(pSlist->size != 0) {
        remove_clib_slist ( pSlist, 0 );
    }
    free ( pSlist );
}

clib_error           
push_back_clib_slist( struct clib_slist *pSlist, void *elem, size_t elem_size){

    struct clib_slist_node* current  = (struct clib_slist_node*)0;
    struct clib_slist_node* new_node = (struct clib_slist_node*)0;

    new_node = (struct clib_slist_node*)malloc(sizeof(struct clib_slist_node));

    new_node->elem = new_clib_object ( elem, elem_size );
    if ( ! new_node->elem )
        return CLIB_SLIST_INSERT_FAILED;
    new_node->next = (struct clib_slist_node*)0;

    if ( pSlist->head == (struct clib_slist_node*)0 ) {
        pSlist->head = new_node;
        pSlist->size++;
        return CLIB_ERROR_SUCCESS;
    }
    current = pSlist->head;
    while ( current->next != (struct clib_slist_node*)0 )
        current  = current->next;    
    current->next = new_node;
    pSlist->size++;

    return CLIB_ERROR_SUCCESS;
}
static void 
pvt_remove_clib_list ( struct clib_slist *pSlist, struct clib_slist_node* pSlistNode ) {
    void *elem;
    get_raw_clib_object(pSlistNode->elem, &elem);
    if ( pSlist->destruct_fn) {             
        (pSlist->destruct_fn)(elem);
        delete_clib_object ( pSlistNode->elem );
    }else {
        free ( elem );
        delete_clib_object ( pSlistNode->elem );
    }        
    free ( pSlistNode);
}
void           
remove_clib_slist( struct clib_slist *pSlist, int pos ) {
    int i = 0;

    struct clib_slist_node* current = pSlist->head;
    struct clib_slist_node* temp    = (struct clib_slist_node*)0;

    if ( pos > pSlist->size ) return;

    if ( pos == 0 ) {                
        pSlist->head = current->next;    
        pvt_remove_clib_list(pSlist, current);    
        pSlist->size--;
        return;
    }
    for ( i = 1; i < pos - 1; i++)
        current = current->next;

    temp          = current->next;
    current->next = current->next->next;
    pvt_remove_clib_list ( pSlist, temp );

    pSlist->size--;
}
clib_error           
insert_clib_slist(struct clib_slist *pSlist, int pos, void *elem, size_t elem_size) {
    int i = 0;
    struct clib_slist_node* current  = pSlist->head;
    struct clib_slist_node* new_node = (struct clib_slist_node*)0;
   
    if ( pos == 1 ) {
        new_node       = (struct clib_slist_node*)malloc(sizeof(struct clib_slist_node));
        new_node->elem = new_clib_object ( elem, elem_size );
        if ( ! new_node->elem ) {
            free ( new_node );
            return CLIB_SLIST_INSERT_FAILED;
        }
        new_node->next = pSlist->head;
        pSlist->head       = new_node;
        pSlist->size++;
        return CLIB_ERROR_SUCCESS;
    }

    if ( pos >= pSlist->size + 1 ) {
        return push_back_clib_slist ( pSlist, elem, elem_size );
    }

    for ( i = 1; i < pos - 1; i++) {
        current = current->next;
    }
    new_node       = (struct clib_slist_node*)malloc(sizeof(struct clib_slist_node));
    new_node->elem = new_clib_object ( elem, elem_size );
    if ( ! new_node->elem ) {
        free ( new_node );
        return CLIB_SLIST_INSERT_FAILED;
    }

    new_node->next = current->next;
    current->next  = new_node;
    pSlist->size++;

    return CLIB_ERROR_SUCCESS;
}
void           
for_each_clib_slist (struct clib_slist *pSlist, void (*fn)(void* )) {
    void *elem;
    struct clib_slist_node* current  = pSlist->head;
    while ( current != (struct clib_slist_node*)0 ) {
        get_raw_clib_object(current->elem, &elem);
        (fn)(elem);
        free ( elem );
        current = current->next;
    }    
}
clib_bool
find_clib_slist (struct clib_slist *pSlist, void* find_value, void**out_value) {
    struct clib_slist_node* current  = pSlist->head;  
    while ( current != (struct clib_slist_node*)0 ) {        
        get_raw_clib_object(current->elem, out_value);
        if ((pSlist->compare_fn)(find_value,*out_value) != 0){
            break;
        }
        free ( *out_value );
        current = current->next;
    }
    if ( current ) {
        return clib_true;
    }
    return clib_false;
}


