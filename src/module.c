#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>

eth_module*
eth_create_module(const char *name)
{
  eth_module *mod = malloc(sizeof(eth_module));
  mod->name = strdup(name);
  mod->ndefs = 0;
  mod->defscap = 0x10;
  mod->defs = malloc(sizeof(eth_def) * mod->defscap);
  return mod;
}

void
eth_destroy_module(eth_module *mod)
{
  free(mod->name);
  for (int i = 0; i < mod->ndefs; ++i)
  {
    free(mod->defs[i].ident);
    eth_unref(mod->defs[i].val);
  }
  free(mod->defs);
  free(mod);
}

void
eth_define(eth_module *mod, const char *ident, eth_t val)
{
  if (eth_unlikely(mod->ndefs == mod->defscap))
  {
    mod->defscap <<= 1;
    mod->defs = realloc(mod->defs, sizeof(eth_def) * mod->defscap);
  }
  mod->defs[mod->ndefs++] = (eth_def) { strdup(ident), val };
  eth_ref(val);
}
