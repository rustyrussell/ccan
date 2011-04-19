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

#include <stdio.h>
extern void test_clib_array();
extern void test_clib_deque();
extern void test_clib_tree();
extern void test_clib_rb();
extern void test_clib_set();
extern void test_clib_map();
extern void test_clib_slist();
extern void test_clib_map();
extern void test_clib_stack();
extern void test_clib_heap();

int main( int argc, char**argv ) {	
    printf ( "Performing test for dynamic array\n");
    test_clib_array();
    printf ( "Performing test for deque\n");
    test_clib_deque();
    printf ( "Performing test for sets\n");
    test_clib_set();
    printf ( "Performing test for map\n");
    test_clib_map();
    printf ( "Performing test for slist\n");
    test_clib_slist();
    printf ( "Performing test for stackn");
    test_clib_stack();
    printf ( "Performing test for heap\n");
    test_clib_heap();


    return 0;
}
