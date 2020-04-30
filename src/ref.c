#include "ether/ether.h"

eth_type *eth_ref_type;

static void
destroy_ref(eth_type *type, eth_t x)
{
  eth_unref(eth_ref_get(x));
  eth_free_h1(x);
}

void
_eth_init_ref_type(void)
{
  eth_ref_type = eth_create_type("ref");
  eth_ref_type->destroy = destroy_ref;
}

eth_t
eth_create_ref(eth_t init)
{
  eth_mut_ref *ref = eth_alloc_h1();
  eth_init_header(ref, eth_ref_type);
  eth_ref(ref->val = init);
  return ETH(ref);
}

