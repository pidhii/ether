#include "ether/ether.h"

static eth_t
_create(void)
{
  eth_t x = *eth_sp++;
  eth_t ref = eth_create_ref(x);
  return ref;
}

static eth_t
_get(void)
{
  eth_use_symbol(Type_error)
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_ref_type))
  {
    eth_drop(x);
    return eth_exn(Type_error);
  }
  eth_t ret = eth_ref_get(x);
  eth_ref(ret);
  eth_drop(x);
  eth_dec(ret);
  return ret;
}

static eth_t
_set(void)
{
  eth_args args = eth_start(2);
  eth_t ref = eth_arg2(args, eth_ref_type);
  eth_t x = eth_arg(args);
  eth_set_ref(ref, x);
  eth_return(args, eth_nil);
}

int
ether_module(eth_module *mod)
{
  eth_define(mod, "create", eth_create_proc(_create, 1, NULL, NULL));
  eth_define(mod, "get", eth_create_proc(_get, 1, NULL, NULL));
  eth_define(mod, "set", eth_create_proc(_set, 2, NULL, NULL));

  return 0;
}
