#if !defined(_egcd_H)
# define _egcd_H (1)

/*Computes the coefficients of the smallest positive linear combination of two
   integers _a and _b.
  These are a solution (u,v) to the equation _a*u+_b*v==gcd(_a,_b).
  _a: The first integer of which to compute the extended gcd.
  _b: The second integer of which to compute the extended gcd.
  _u: Returns the coefficient of _a in the smallest positive linear
       combination.
  _v: Returns the coefficient _of b in the smallest positive linear
       combination.
  Return: The non-negative gcd of _a and _b.
          If _a and _b are both 0, then 0 is returned, though in reality the
           gcd is undefined, as any integer, no matter how large, will divide 0
           evenly.
          _a*u+_b*v will not be positive in this case.
          Note that the solution (u,v) of _a*u+_b*v==gcd(_a,_b) returned is not
           unique.
          (u+(_b/gcd(_a,_b))*k,v-(_a/gcd(_a,_b))*k) is also a solution for all
           k.
          The coefficients (u,v) might not be positive.*/
int egcd(int _a,int _b,int *_u,int *_v);

#endif
