#include "ether/ether.h"

#include <stdlib.h>


void
eth_destroy_spec(eth_spec* spec)
{
  switch (spec->tag)
  {
    case ETH_SPEC_TYPE:
      free(spec);
      break;
  }
}

eth_spec*
eth_create_type_spec(int varid, eth_type *type)
{
  eth_spec *spec = eth_malloc(sizeof(eth_spec));
  spec->tag = ETH_SPEC_TYPE;
  spec->type_spec.varid = varid;
  spec->type_spec.type = type;
  return spec;
}


