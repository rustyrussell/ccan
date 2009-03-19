#include "crt.h"
#include "egcd.h"

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
int crt(int *_x,const int _a[],const int _m[],int _n){
  int i;
  int m0;
  int a0;
  int x0;
  /*If there are no equations, everything is a solution.*/
  if(_n<1){
    *_x=0;
    return 1;
  }
  /*Start with the first equation.*/
  /*Force all moduli to be positive.*/
  m0=_m[0]<0?-_m[0]:_m[0];
  a0=_a[0];
  /*Add in each additional equation, and use it to derive a new, single
     equation.*/
  for(i=1;i<_n;i++){
    int d;
    int mi;
    int xi;
    /*Force all moduli to be positive.*/
    mi=_m[i]<0?-_m[i]:_m[i];
    /*Compute the inverse of m0 (mod mi) and of mi (mod m0).
      These are only inverses if m0 and mi are relatively prime.*/
    d=egcd(m0,mi,&xi,&x0);
    if(d>1){
      /*The hard case: m0 and mi are not relatively prime.*/
      /*First: check for consistency.*/
      if((a0-_a[i])%d!=0)return -1;
      /*If m0 divides mi, the old equation was completely redundant.*/
      else if(d==m0){
        a0=_a[i];
        m0=mi;
      }
      /*If mi divides m0, the new equation is completely redundant.*/
      else if(d!=mi){
        /*Otherwise the two have a non-trivial combination.
          The system is equivalent to the system
            x == a0 (mod m0/d)
            x == a0 (mod d)
            x == ai (mod mi/d)
            x == ai (mod d)
          Note that a0+c*(ai-a0) == a0 (mod d) == ai (mod d) for any c, since
           d|ai-a0; thus any such c gives solutions that satisfy eqs. 2 and 4.
          Choosing c as a multiple of (m0/d) ensures
            a0+c*(ai-a0) == a0 (mod m0/d), satisfying eq. 1.
          But (m0/d) and (mi/d) are relatively prime, so we can choose
            c = (m0/d)*((m0/d)^-1 mod mi/d),
          Hence c == 1 (mod mi/d), and
            a0+c*(ai-a0) == ai (mod mi/d), satisfying eq. 3.
          The inverse of (m0/d) mod (mi/d) can be computed with the egcd().*/
        m0/=d;
        egcd(m0,mi/d,&xi,&x0);
        a0+=(_a[i]-a0)*m0*xi;
        m0*=mi;
        a0=a0%m0;
      }
    }
    else if(d==1){
      /*m0 and mi are relatively prime, so xi and x0 are the inverses of m0 and
         mi modulo mi and m0, respectively.
        The Chinese Remainder Theorem now states that the solution is given by*/
      a0=a0*mi*x0+_a[i]*m0*xi;
      /* modulo the LCM of m0 and mi.*/
      m0*=mi;
      a0%=m0;
    }
    /*Special case: mi and m0 are both 0.
      Check for consistency.*/
    else if(a0!=_a[i])return -1;
    /*Otherwise, this equation was redundant.*/
  }
  /*If the final modulus was not 0, then constrain the answer to be
     non-negative and less than that modulus.*/
  if(m0!=0){
    x0=a0%m0;
    *_x=x0<0?x0+m0:x0;
  }
  /*Otherwise, there is only one solution.*/
  else *_x=a0;
  return m0;
}
