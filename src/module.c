#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>


ETH_MODULE("ether:module")


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
  /*eth_debug("destroying module %s:", mod->name);*/
  free(mod->name);
  for (int i = 0; i < mod->ndefs; ++i)
  {
    /*eth_debug("- delete '%s'", mod->defs[i].ident);*/
    free(mod->defs[i].ident);
    eth_unref(mod->defs[i].val);
    eth_unref_attr(mod->defs[i].attr);
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
eth_define_attr(eth_module *mod, const char *ident, eth_t val, eth_attr *attr)
{
  if (eth_unlikely(mod->ndefs == mod->defscap))
  {
    mod->defscap <<= 1;
    mod->defs = realloc(mod->defs, sizeof(eth_def) * mod->defscap);
  }
  eth_def def =  {
    .ident = strdup(ident),
    .val = val,
    .attr = attr,
  };
  mod->defs[mod->ndefs++] = def;
  eth_ref_attr(def.attr);
  eth_ref(val);
}


void
eth_define(eth_module *mod, const char *ident, eth_t val)
{
  eth_define_attr(mod, ident, val, eth_create_attr());
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

