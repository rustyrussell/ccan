#include <stdint.h>
/* returns 0 for little-endian, 1 for big-endian */
int main( void )
{
  static const uint64_t one = (uint64_t) 1;
  if ((uint8_t) 1 == *((uint8_t *) &one))
    return 0;
  else
    return 1;
}
