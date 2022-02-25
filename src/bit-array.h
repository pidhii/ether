#ifndef ETHER_BIT_ARRAY
#define ETHER_BIT_ARRAY

#include <stdint.h>
#include <stdlib.h>

/*
 * Layout of the array is the following:
 *
 *  |8|7|...|1|16|15|...|9|...
 */

static inline int
bit_array_size(int nbits)
{
  div_t d = div(nbits, 8);
  return (d.quot + !!d.rem) << 3;
}

static inline void
set_bit(uint8_t arr[], int k)
{
  div_t d = div(k, 8);
  arr[d.quot] = 1 << d.rem;
}

static inline void
clear_bit(uint8_t arr[], int k)
{
  div_t d = div(k, 8);
  arr[d.quot] &= ~(1 << d.rem);
}

static inline int
test_bit(uint8_t arr[], int k)
{
  div_t d = div(k, 8);
  return !!(arr[d.quot] & (1 << d.rem));
}

#endif
