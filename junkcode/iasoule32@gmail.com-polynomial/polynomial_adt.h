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
#include <stdbool.h> //C99 compliance
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

void display_poly(const PolyAdt *a);
/**
* create_adt - create a polynomial on the heap
* @hp: the highest power in the polynomial
*/
PolyAdt *create_adt(int hp) 
{
    PolyAdt *pAdt = malloc(sizeof(PolyAdt));
    assert(pAdt != NULL);
    
    pAdt->head = NULL;
    pAdt->terms = 0;
    pAdt->hp = hp;

    return pAdt;
}
/**
* create_node - creates a Node (exponent, constant and next pointer) on the heap 
* @constant: the contant in the term 
* @exp:      the exponent on the term
* @next:     the next pointer to another term in the polynomial
* 
* This should not be called by client code (hence marked static)
* used to assist insert_term()
*/ 
static Node *create_node(float constant, int exp, Node *next)
{
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
void insert_term(PolyAdt *pAdt, float c, int e)
{
    assert(pAdt != NULL); //assume client code didnt call create_adt()
    Node *n = malloc(sizeof(Node));
       
    if(pAdt->head == NULL)
        pAdt->head = create_node(c, e, pAdt->head);
    else
        for(n = pAdt->head; n->next != NULL; n = n->next); //go to the end of list
            n->next = create_node(c, e, NULL);
    
    pAdt->terms++;
}

/**
* polyImage - returns an image (direct) copy of the polynomial
* @orig: the polynomial to be duplicated
*/
PolyAdt *polyImage(const PolyAdt *orig)
{
    PolyAdt *img = create_adt(orig->hp);
    Node *origHead = orig->head;
    
    for(; origHead; origHead = origHead->next)
             insert_term(img, origHead->coeff, origHead->exp);
    return img;
}


/**
* add - adds two polynomials together, and returns their sum (as a polynomial)
* @a: the 1st polynomial 
* @b: the 2nd polynomial
*/
PolyAdt *add(const PolyAdt *a, const PolyAdt *b)
{
    PolyAdt *sum;
    Node *n, *np;
    _Bool state = true;
    
    assert(a != NULL && b != NULL);
    
    int hpow = max(a->hp, b->hp);
    sum = create_adt(hpow); //create space for it
    
    /* using state machine to compare the poly with the most terms to 
    ** the poly with fewer, round robin type of effect comparison of
    ** exponents => 3 Cases: Equal, Less, Greater
    */
        n = a->head; np = b->head;
        while(state) {
            /* compare the exponents */
            if(n->exp == np->exp){
                insert_term(sum, n->coeff + np->coeff, n->exp);
                n = n->next;
                np = np->next;
            }
            
            else if(n->exp < np->exp){
                insert_term(sum, np->coeff, np->exp);
                np = np->next; //move to next term of b
            }
            
            else { //greater than
                insert_term(sum, n->coeff, n->exp);
                n = n->next;
            }
            /* check whether at the end of one list or the other */
            if(np == NULL && state == true){ //copy rest of a to sum
                for(; n != NULL; n = n->next)
                    insert_term(sum, n->coeff, n->exp);
                state = false;
            }
            
           if(n == NULL && state == true){
                for(; np != NULL; np = np->next)
                    insert_term(sum, np->coeff, np->exp);
                state = false;
            }       
     }        
    return sum;               
}            

/**
* sub - subtracts two polynomials, and returns their difference (as a polynomial)
* @a: the 1st polynomial 
* @b: the 2nd polynomial
* Aids in code reuse by negating the terms (b) and then calls the add() function
*/
PolyAdt *subtract(const PolyAdt *a, const PolyAdt *b)
{
	assert(a != NULL && b != NULL);

    PolyAdt *tmp = create_adt(b->hp);
    Node *bptr;
    
    for(bptr = b->head; bptr != NULL; bptr = bptr->next)
        insert_term(tmp,-bptr->coeff,bptr->exp);  //negating b's coeffs
    return add(a,tmp);
}

/**
* multiply - multiply two polynomials, and returns their product (as a polynomial)
* @a: the 1st polynomial 
* @b: the 2nd polynomial
*/
PolyAdt *multiply(const PolyAdt *a, const PolyAdt *b)
{
	assert(a != NULL && b != NULL);

    //the polys are inserted in order for now
    PolyAdt *prod = create_adt(a->head->exp + b->head->exp);
    Node *n = a->head, *np = b->head;
    Node *t = b->head; 
    
    if(a->terms < b->terms){
        n = b->head;
        np = t = a->head;
    }
    
    for(; n != NULL; n = n->next){
        np = t; //reset to the beginning
        for(; np != NULL; np = np->next){ //always the least term in this loop
                insert_term(prod, n->coeff * np->coeff, n->exp + np->exp);
        }
    }

    return prod;       
}

/**
* derivative - computes the derivative of a polynomial and returns the result
* @a: the polynomial to take the derivative upon
*/
PolyAdt *derivative(const PolyAdt *a)
{
	assert(a != NULL);
	
	PolyAdt *deriv = create_adt(a->head->exp - 1);
	Node *n = a->head;

	for(; n != NULL; n = n->next){
		if(n->exp == 0) break;
		insert_term(deriv, n->coeff * n->exp, n->exp-1);
	}
	return deriv;
}
/**
* integrate - computes the integral of a polynomial and returns the result
* @a: the polynomial to take the integral of
* 
* Will compute an indefinite integral over a
*/
PolyAdt *integrate(const PolyAdt *a)
{
	assert(a != NULL);
	
	PolyAdt *integrand = create_adt(a->head->exp + 1);
	Node *n;

	for(n = a->head; n != NULL; n = n->next) //very simple term by term
        insert_term(integrand, (float)n->coeff/(n->exp+1.0F), n->exp + 1);
    
	return integrand;
}
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
void quadratic_roots(const PolyAdt *a, float *real, float *cplx)
{
	assert(a != NULL);
	
	float dscrmnt, _a, b, c;
	float u, v;
    
    Node *n = a->head;
    _a = n->coeff; b = n->next->coeff; c = n->next->next->coeff;
    
	dscrmnt = (b*b) - 4*_a*c;
    u = -b/(2*_a); v = sqrt((double)fabs(dscrmnt))/(2*_a);
    
	if(real && !cplx || !real && cplx)
		assert(true);

	if(real == NULL && cplx == NULL){
		if(a->hp != 2 && a->terms < 3){
		  printf("Invalid Quadratic*, A and B must be non-zero");
			return;
        }
        
		if(dscrmnt != 0)
			printf("X = %.2f +/- %.2f%c\n",u,v,dscrmnt < 0 ? 'I':' ');
		else{
			printf("(X %c %.2f)(X %c %.2f)\n",sgn(u),fabs(u),sgn(u),fabs(u));
			printf("X1,2 = %.2f\n",u);
		}
	}
	//save values in pointers
	else {
		if(dscrmnt < 0){ //x = u +/- vI Re(x) = u, Im(x) = +v
			*real = u; 
			*cplx = v; //understand +/- is not representable
		}
		else if(dscrmnt == 0){
			*real = u; 
			*cplx = 0.00F;
		}
		else{
			*real = u + v;
			*cplx = u - v;
		}
	}
}

/**
* exponentiate - computes polynomial exponentiation (P(x))^n, n E Z*
* @a: the polynomial
* @n: the exponent
* Works fast for small n (n < 8) currently runs ~ O(n^2 lg n)
*/
PolyAdt *exponentiate(const PolyAdt *a, int n)
{
	assert(a != NULL);

	PolyAdt *expn = create_adt(a->hp *  n);
	PolyAdt *aptr = polyImage(a);
    int hl = n / 2;
    
    //check default cases before calculation
    if(n == 0){
        insert_term(expn, 1, 0);
        return expn;
    }
    else if(n == 1){
        return aptr;
    }
        
	for(; hl ; hl--)
        aptr = multiply(aptr, aptr);

    if(n % 2) //odd exponent do a^(n-1) * a = a^n
        expn = multiply(aptr, a);
    else
        expn = aptr;
    return expn;
}
/**
* compose - computes the composition of two polynomials P(Q(x)) and returns the composition
* @p: polynomial P(x) which will x will be equal to Q(x)
* @q: polynomial Q(x) which is the argument to P(x)
*/
PolyAdt *compose(const PolyAdt *p, const PolyAdt *q)
{
    assert(p && q);
    
	PolyAdt *comp = create_adt(p->head->exp * q->head->exp);
	PolyAdt *exp;
	
    Node *pp = p->head;
    Node *qq = q->head;
    
    int swap = 0;
    
    if(p->terms < q->terms){
        pp = q->head;
        qq = p->head;
        swap = 1;
    }
    
    /* going through, exponentiate each term with the exponent of p */
        for(; pp != NULL; pp = pp->next){
                exp = exponentiate(swap ? p: q, pp->exp);
                insert_term(comp, pp->coeff * exp->head->coeff, exp->head->exp);
        }
    
    return comp;
}
/** 
* destroy_poly - completely frees the polynomial from the heap and resets all values
* @poly: the polynomial to release memory back to the heap
* Usage:
* destroy_poly(myPoly); //puts polynomial on free list
*/
void destroy_poly(PolyAdt *poly)
{
    Node *ps = poly->head;
    Node *tmp = NULL;
    while(ps != NULL){
        tmp = ps;
        free(tmp);
        ps = ps->next;
    }
    poly->hp = poly->terms = 0;
    poly->head = NULL;
}
/**
* display_poly - displays the polynomial to the console in nice format
* @a: the polynomial to display 
*/
void display_poly(const PolyAdt *a)
{
    assert(a != NULL);
    Node *n;
    
    for(n = a->head; n != NULL; n = n->next){
        
       n->coeff < 0 ? putchar('-') : putchar('+'); 
        if(n->exp == 0)
            printf(" %.2f ",fabs(n->coeff));
        else if(n->coeff == 1)
            printf(" X^%d ",n->exp);
        else if(n->exp == 1)
            printf(" %.2fX ",fabs(n->coeff));
        else if(n->coeff == 0)
            continue;
        else
            printf(" %.2fX^%d ",fabs(n->coeff),n->exp);
        }
    printf("\n\n");
}

#endif
