/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_ALLOC_BITOPS_H
#define CCAN_ALLOC_BITOPS_H
unsigned int afls(unsigned long val);
unsigned int affsl(unsigned long val);
unsigned int popcount(unsigned long val);
unsigned long align_up(unsigned long x, unsigned long align);
#endif /* CCAN_ALLOC_BITOPS_H */
