#include "ether/ether.h"

eth_type *eth_nil_type;
eth_t eth_nil;

static void
write_nil(eth_type *type, eth_t x, FILE *out)
{
  fputs("nil", out);
}

void
_eth_init_nil_type(void)
{
  static eth_header nil;

  eth_nil_type = eth_create_type("nil");
  eth_nil_type->write = write_nil;
  eth_nil_type->display = write_nil;

  eth_nil = &nil;
  eth_init_header(eth_nil, eth_nil_type);
  eth_ref(eth_nil);
}
