#include "ether/ether.h"

#include <ctype.h>

#define IMPL(fn) \
  eth_t \
  _##fn(void) \
  { \
    eth_t x = *eth_sp++; \
    if (not eth_is_num(x)) \
    { \
      eth_drop(x); \
      return eth_exn(eth_type_error()); \
    } \
    int c = eth_num_val(x); \
    eth_drop(x); \
    return eth_boolean(fn(c)); \
  }

IMPL(isalnum)
IMPL(isalpha)
IMPL(iscntrl)
IMPL(isdigit)
IMPL(isgraph)
IMPL(islower)
IMPL(isprint)
IMPL(ispunct)
IMPL(isspace)
IMPL(isupper)
IMPL(isxdigit)
IMPL(isascii)
IMPL(isblank)

int
ether_module(eth_module *mod, eth_root *topenv)
{
#define DEF(fn) eth_define(mod, "__" #fn, eth_proc(_##fn, 1))

  DEF(isalnum);
  DEF(isalpha);
  DEF(iscntrl);
  DEF(isdigit);
  DEF(isgraph);
  DEF(islower);
  DEF(isprint);
  DEF(ispunct);
  DEF(isspace);
  DEF(isupper);
  DEF(isxdigit);
  DEF(isascii);
  DEF(isblank);

  eth_module *script = eth_load_module_from_script2(topenv, "./lib.eth", NULL, mod);
  if (not script)
    return -1;
  eth_copy_defs(script, mod);
  eth_destroy_module(script);

  return 0;
}
