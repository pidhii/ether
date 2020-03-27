#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>

eth_environment*
eth_create_environment(void)
{
  eth_environment *env = malloc(sizeof(eth_environment));
  env->nmods = 0;
  env->modscap = 0x10;
  env->mods = malloc(sizeof(eth_module*) * env->modscap);
  return env;
}

void
eth_destroy_environment(eth_environment *env)
{
  for (int i = 0; i < env->nmods; ++i)
    eth_destroy_module(env->mods[i]);
  free(env->mods);
  free(env);
}

void
eth_add_module(eth_environment *env, eth_module *mod)
{
  if (eth_unlikely(env->nmods == env->modscap))
  {
    env->modscap <<= 1;
    env->mods = realloc(env->mods, sizeof(eth_module*) * env->modscap);
  }
  env->mods[env->nmods++] = mod;
}

