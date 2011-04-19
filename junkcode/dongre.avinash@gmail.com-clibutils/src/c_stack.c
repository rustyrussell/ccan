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

struct clib_stack *
new_clib_stack( int default_size, clib_destroy fn_d) {
	struct clib_stack *pStack = ( struct clib_stack*)malloc(sizeof ( struct clib_stack));
	pStack->pStackArr = new_clib_array ( default_size, NULL, fn_d);
	return pStack;
}
void 
delete_clib_stack(struct clib_stack *pStack){
	if ( pStack ){
		delete_clib_array ( pStack->pStackArr );
	}
	free ( pStack );
}
void 
push_clib_stack(struct clib_stack *pStack, void *elem, size_t elem_size) {
	push_back_clib_array( pStack->pStackArr, elem, elem_size);
}
void 
pop_clib_stack(struct clib_stack *pStack, void **elem) {
	back_clib_array( pStack->pStackArr, elem );
	remove_clib_array( pStack->pStackArr, size_clib_array( pStack->pStackArr) - 1);
}
clib_bool 
empty_clib_stack ( struct clib_stack *pStack) {
	return empty_clib_array( pStack->pStackArr);
}
