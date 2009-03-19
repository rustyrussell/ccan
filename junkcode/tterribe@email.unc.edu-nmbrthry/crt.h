#if !defined(_crt_H)
# define _crt_H (1)

/*Computes the solution to a system of simple linear congruences via the
   Chinese Remainder Theorem.
  This function solves the system of equations
   x = a_i (mod m_i)
  A value x satisfies this equation if there exists an integer k such that
   x = a_i + k*m_i
  Note that under this definition, negative moduli are equivalent to positive
   moduli, and a modulus of 0 demands exact equality.
  x: Returns the solution, if it exists.
     Otherwise, the value is unchanged.
  a: An array of the a_i's.
  m: An array of the m_i's.
     These do not have to be relatively prime.
  n: The number of equations in the system.
  Return: -1 if the system is not consistent, otherwise the modulus by which
           the solution is unique.
          This modulus is the LCM of the m_i's, except in the case where one of
           them is 0, in which case the return value is also 0.*/
int crt(int *_x,const int _a[],const int _m[],int _n);

#endif
