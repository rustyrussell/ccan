#if !defined(_choose_H)
# define _choose_H (1)

/*Computes the number of combinations of _n items, taken _m at a time without
   overflow.
  _n: The total number of items.
  _b: The number taken at a time.
  Return: The number of combinations of _n items taken _m at a time.*/
unsigned choose(int _n,int _m);

#endif
