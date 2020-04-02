#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>

// TODO: use hash table
struct eth_module {
  char *name;
  int ndefs, defscap;
  eth_def *defs;
  eth_env *env;
};

eth_module*
eth_create_module(const char *name)
{
  eth_module *mod = malloc(sizeof(eth_module));
  mod->name = strdup(name);
  mod->ndefs = 0;
  mod->defscap = 0x10;
  mod->defs = malloc(sizeof(eth_def) * mod->defscap);
  mod->env = eth_create_empty_env();
  eth_add_module_path(mod->env, ".");
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
  eth_destroy_env(mod->env);
  free(mod);
}

const char*
eth_get_module_name(const eth_module *mod)
{
  return mod->name;
}

int
eth_get_ndefs(const eth_module *mod)
{
  return mod->ndefs;
}

eth_def*
eth_get_defs(const eth_module *mod, eth_def out[])
{
  memcpy(out, mod->defs, sizeof(eth_def) * mod->ndefs);
  return out;
}

eth_env*
eth_get_env(const eth_module *mod)
{
  return mod->env;
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

eth_def*
eth_find_def(const eth_module *mod, const char *ident)
{
  for (int i = 0; i < mod->ndefs; ++i)
  {
    if (strcmp(mod->defs[i].ident, ident) == 0)
      return mod->defs + i;
  }
  return NULL;
}

