#ifndef CCAN_ALLOC_BITOPS_H
#define CCAN_ALLOC_BITOPS_H
unsigned int fls(unsigned long val);
unsigned int ffsl(unsigned long val);
unsigned int popcount(unsigned long val);
unsigned long align_up(unsigned long x, unsigned long align);
#endif /* CCAN_ALLOC_BITOPS_H */
