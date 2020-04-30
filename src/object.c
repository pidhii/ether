#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t key;
  eth_t val;
} field;

struct eth_object {
  eth_header header;
  size_t nparents;
  eth_t *parents;
  size_t nfields;
  field *fields; /* Sorted by id in increasing order.
                    Capacity of this array at least `nfields + 1`. */
};


