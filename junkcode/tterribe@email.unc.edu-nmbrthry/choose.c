#include "choose.h"
#include "gcd.h"

/*Computes the number of combinations of _n items, taken _m at a time without
   overflow.
  _n: The total number of items.
  _m: The number taken at a time.
  Return: The number of combinations of _n items taken _m at a time.*/
unsigned choose(int _n,int _m){
  unsigned ret;
  int      i;
  ret=1;
  for(i=1;i<=_m;_n--,i++){
    int nmi;
    nmi=_n%i;
    if(nmi==0)ret*=_n/i;
    else if(ret%i==0)ret=(ret/i)*_n;
    else{
      int d;
      d=gcd(i,nmi);
      ret=(ret/(i/d))*(_n/d);
    }
  }
  return ret;
}
