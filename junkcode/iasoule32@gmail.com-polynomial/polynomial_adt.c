#include "polynomial_adt.h"

PolyAdt *create_adt(int hp)
{
    PolyAdt *pAdt = malloc(sizeof(PolyAdt));
    assert(pAdt != NULL);
    
    pAdt->head = NULL;
    pAdt->terms = 0;
    pAdt->hp = hp;

    return pAdt;
}

void insert_term(PolyAdt *pAdt, float c, int e)
{
    assert(pAdt != NULL); //assume client code didn't call create_adt()
    Node *n = malloc(sizeof(Node));
       
    if(pAdt->head == NULL)
        pAdt->head = create_node(c, e, pAdt->head);
    else
        for(n = pAdt->head; n->next != NULL; n = n->next); //go to the end of list
            n->next = create_node(c, e, NULL);
    
    pAdt->terms++;
}

PolyAdt *polyImage(const PolyAdt *orig)
{
    PolyAdt *img = create_adt(orig->hp);
    Node *origHead = orig->head;
    
    for(; origHead; origHead = origHead->next)
             insert_term(img, origHead->coeff, origHead->exp);
    return img;
}

PolyAdt *add(const PolyAdt *a, const PolyAdt *b)
{
    PolyAdt *sum;
    Node *n, *np;
    int state = 1;
    
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
            if(np == NULL && state){ //copy rest of a to sum
                for(; n != NULL; n = n->next)
                    insert_term(sum, n->coeff, n->exp);
                state = 0;
            }
            
           if(n == NULL && state){
                for(; np != NULL; np = np->next)
                    insert_term(sum, np->coeff, np->exp);
                state = 0;
            }
     }
    return sum;
}

PolyAdt *subtract(const PolyAdt *a, const PolyAdt *b)
{
	assert(a != NULL && b != NULL);

    PolyAdt *tmp = create_adt(b->hp);
    Node *bptr;
    
    for(bptr = b->head; bptr != NULL; bptr = bptr->next)
        insert_term(tmp,-bptr->coeff,bptr->exp);  //negating b's coeffs
    return add(a,tmp);
}

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

PolyAdt *integrate(const PolyAdt *a)
{
	assert(a != NULL);
	
	PolyAdt *integrand = create_adt(a->head->exp + 1);
	Node *n;

	for(n = a->head; n != NULL; n = n->next) //very simple term by term
        insert_term(integrand, (float)n->coeff/(n->exp+1.0F), n->exp + 1);
    
	return integrand;
}

void quadratic_roots(const PolyAdt *a, float *real, float *cplx)
{
	assert(a != NULL);
	
	float dscrmnt, _a, b, c;
	float u, v;
    
    Node *n = a->head;
    _a = n->coeff; b = n->next->coeff; c = n->next->next->coeff;
    
	dscrmnt = (b*b) - 4*_a*c;
    u = -b/(2*_a); v = sqrt((double)fabs(dscrmnt))/(2*_a);
    
	if((real && !cplx) || (!real && cplx))
		assert(1);

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
