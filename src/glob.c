#include "ether/ether.h"


ETH_MODULE("ether:glob");


eth_type *eth_glob_type;


static void
destroy_glob(eth_type *type, eth_t glob)
{
  eth_unref(((eth_glob*)glob)->rl);
  eth_free(glob, sizeof(eth_glob));
}

eth_t
get_impl(void)
{
  eth_args args = eth_start(2);
  eth_t glob = eth_arg2(args, eth_glob_type);
  eth_t sym = eth_arg2(args, eth_symbol_type);
  eth_t ret = eth_glob_find(glob, sym);
  if (ret)
    eth_return(args, ret);
  else
  {
    eth_error("no such variable, ~d", sym);
    abort();
  }
}

void
_eth_init_glob_type(void)
{
  eth_glob_type = eth_create_type("glob");
  eth_glob_type->destroy = destroy_glob;
  eth_add_method(eth_glob_type->methods, eth_get_method, eth_proc(get_impl, 2));
}

eth_t
eth_create_glob()
{
  eth_glob *glob = eth_alloc(sizeof(eth_glob));
  eth_ref(glob->rl = eth_nil);
  eth_init_header(&glob->hdr, eth_glob_type);
  return ETH(glob);
}

eth_t
eth_glob_add(eth_t glob, eth_t r)
{
  eth_t oldrl = ((eth_glob*)glob)->rl;

  eth_glob *newglob = eth_alloc(sizeof(eth_glob));
  eth_ref(newglob->rl = eth_cons(r, oldrl));
  eth_init_header(&newglob->hdr, eth_glob_type);
  return ETH(newglob);
}

eth_t
eth_glob_find(eth_t glob, eth_t sym)
{
  eth_t rl = ((eth_glob*)glob)->rl;
  for (eth_t it = rl; it->type == eth_pair_type; it = eth_cdr(it))
  {
    eth_t r = eth_car(it);
    int i = eth_get_field_by_id(r->type, eth_get_symbol_id(sym));
    if (i < r->type->nfields)
      return eth_tup_get(r, i);
  }
  return NULL;
}

