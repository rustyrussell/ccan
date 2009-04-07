/* Polynomial ADT
** A polynomial module with
** ability to add,sub,mul derivate/integrate, compose ... polynomials
** ..expansion in progress ...
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

#ifndef __POLYNOMIAL_ADT
#define __POLYNOMIAL_ADT

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define max(a, b) (a) > (b) ? (a) : (b)
#define sgn(a)    (a) < 0 ? '+' : '-' //for quadratic factored form

typedef struct node {
    int exp;
    float coeff;
    struct node *next;
}Node;

typedef struct polynomial_adt {
    Node *head;
    int terms, hp; //hp highest power
}PolyAdt;

/**
* create_adt - create a polynomial on the heap
* @hp: the highest power in the polynomial
*/
PolyAdt *create_adt(int hp);

/**
* create_node - creates a Node (exponent, constant and next pointer) on the heap
* @constant: the contant in the term
* @exp:      the exponent on the term
* @next:     the next pointer to another term in the polynomial
*
* This should not be called by client code (hence marked static)
* used to assist insert_term()
*/
static inline Node *create_node(float constant, int exp, Node *next) {
	Node *nNode = malloc(sizeof(Node));
    assert(nNode != NULL);
    
    nNode->exp = exp;
    nNode->coeff = constant;
    nNode->next = next;
    return nNode;
}

/**
* insert_term - inserts a term into the polynomial
* @pAdt: the polynomial
* @c:    constant value on the term
* @e:    the exponent on the term
*/
void insert_term(PolyAdt *pAdt, float c, int e);

/**
* polyImage - returns an image (direct) copy of the polynomial
* @orig: the polynomial to be duplicated
*/
PolyAdt *polyImage(const PolyAdt *orig);


/**
* add - adds two polynomials together, and returns their sum (as a polynomial)
* @a: the 1st polynomial
* @b: the 2nd polynomial
*/
PolyAdt *add(const PolyAdt *a, const PolyAdt *b);

/**
* sub - subtracts two polynomials, and returns their difference (as a polynomial)
* @a: the 1st polynomial
* @b: the 2nd polynomial
* Aids in code reuse by negating the terms (b) and then calls the add() function
*/
PolyAdt *subtract(const PolyAdt *a, const PolyAdt *b);

/**
* multiply - multiply two polynomials, and returns their product (as a polynomial)
* @a: the 1st polynomial
* @b: the 2nd polynomial
*/
PolyAdt *multiply(const PolyAdt *a, const PolyAdt *b);

/**
* derivative - computes the derivative of a polynomial and returns the result
* @a: the polynomial to take the derivative upon
*/
PolyAdt *derivative(const PolyAdt *a);

/**
* integrate - computes the integral of a polynomial and returns the result
* @a: the polynomial to take the integral of
*
* Will compute an indefinite integral over a
*/
PolyAdt *integrate(const PolyAdt *a);

/**
* quadratic_roots - finds the roots of the polynomial ax^2+bx+c, a != 0 && b != 0
* @a: the polynomial
* @real: a pointer to float of the real(R) part of a
* @cplx: a pointer to float of the imaginary(I) part of a
*
* Usage:
* Two options can be done by the client
* 1. Either pass NULL to real and cplx
*    this will display the roots by printf
*    quadratic_roots(myPolynomial, NULL, NULL);
*
* 2. Pass in pointers** to type float of the real and complex
*    if the discriminant is >0 cplx = -ve root of X
*    quadratic_roots(myPolynomial, &realPart, &complexPart);
*/
void quadratic_roots(const PolyAdt *a, float *real, float *cplx);

/**
* exponentiate - computes polynomial exponentiation (P(x))^n, n E Z*
* @a: the polynomial
* @n: the exponent
* Works fast for small n (n < 8) currently runs ~ O(n^2 lg n)
*/
PolyAdt *exponentiate(const PolyAdt *a, int n);

/**
* compose - computes the composition of two polynomials P(Q(x)) and returns the composition
* @p: polynomial P(x) which will x will be equal to Q(x)
* @q: polynomial Q(x) which is the argument to P(x)
*/
PolyAdt *compose(const PolyAdt *p, const PolyAdt *q);

/**
* destroy_poly - completely frees the polynomial from the heap and resets all values
* @poly: the polynomial to release memory back to the heap
* Usage:
* destroy_poly(myPoly); //puts polynomial on free list
*/
void destroy_poly(PolyAdt *poly);

/**
* display_poly - displays the polynomial to the console in nice format
* @a: the polynomial to display
*/
void display_poly(const PolyAdt *a);

#endif
