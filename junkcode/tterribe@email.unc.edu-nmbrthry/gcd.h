#if !defined(_gcd_H)
# define _gcd_H (1)

/*Computes the gcd of two integers, _a and _b.
  _a: The first integer of which to compute the gcd.
  _b: The second integer of which to compute the gcd.
  Return: The non-negative gcd of _a and _b.
          If _a and _b are both 0, then 0 is returned, though in reality the
           gcd is undefined, as any integer, no matter how large, will divide 0
           evenly.*/
int gcd(int _a,int _b);

#endif
