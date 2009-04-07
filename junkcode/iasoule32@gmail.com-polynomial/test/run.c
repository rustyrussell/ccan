/* 
** polynomial_adt_test.c
** Test (minimalistic) for the polynomial module
 * More of a display of functionality
 * Copyright (c) 2009 I. Soule
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
**          iasoule32@gmail.com
*/

#include <stdio.h>
#include "../polynomial_adt.h"
#include "../polynomial_adt.c"

int main(void)
{
    PolyAdt *p = create_adt(5), *q = create_adt(4);
    PolyAdt *sum, *diff, *prod;
    
    insert_term(p,1,5);
    insert_term(p,3,4);
    insert_term(p,1,3);
    insert_term(p,9,2);
    insert_term(p,8,1);
    
    insert_term(q,2,4);
    insert_term(q,8,3);
    insert_term(q,7,2);
    insert_term(q,6,1);
    
    
    printf("Displaying Polynomials ...\n");
    display_poly(p);
    display_poly(q);
    
    sum = add(p,q);
    printf("P(x) + Q(x) = ");
    display_poly(sum);
    
    diff = subtract(p,q);
    printf("P(x) - Q(x) = ");
    display_poly(diff);
    
    prod = multiply(p,q);
    printf("P(x)*Q(x) = ");
    display_poly(prod);
    
    PolyAdt *quad = create_adt(2);
    insert_term(quad, 10, 2);
    insert_term(quad, 30, 1);
    insert_term(quad, 2, 0);
    
    quadratic_roots(quad, NULL, NULL); //print out the roots
    
    float real, cplx;
    quadratic_roots(quad, &real, &cplx);
    
    printf("X1 = %f, X2 = %f\n\n", real, cplx);
    
    PolyAdt *deriv, *integral;
    
    deriv = derivative(p);
    printf("The derivitive of p = ");
    display_poly(deriv);
    integral = integrate(q);
    
    printf("The integral of q = ");
    display_poly(integral);
    
    printf("\n Computing P(x)^3\n");
    
    PolyAdt *expo;
    expo = exponentiate(p, 3);
    display_poly(expo);
    printf("\n");
    
    printf("Computing Integral[Q(x)^2]\n");
    expo = exponentiate(q, 2);
    integral = integrate(expo);
    display_poly(integral);
    
    
    printf(" Differentiating and Integrating P\n");
    display_poly(integrate(derivative(p))); 
    
    PolyAdt *L, *M;
    
    L = create_adt(3), M = create_adt(2);
    
    insert_term(L, 4, 3);
    insert_term(L, 10, 2);
    insert_term(L, 15, 1);
    
    insert_term(M, 4, 2);
    printf("L = ");
    display_poly(L);
    printf("M = ");
    display_poly(M);
    
    
    printf("Computing composition L(M(X))\n");
    display_poly(compose(L, M));
    
    printf("Freed memory back to heap for allocated polys'\n");
    destroy_poly(sum);
    destroy_poly(diff);
    destroy_poly(prod);
    destroy_poly(L); destroy_poly(M);
    destroy_poly(q); destroy_poly(p);
    
    return 0;
}
