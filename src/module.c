/* Copyright (C) 2020  Ivan Pidhurskyi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


ETH_MODULE("ether:module")

typedef struct {
  void *data;
  void (*dtor)(void*);
} closure;

typedef struct {
  void (*cb)(void*);
  void *data;
} exit_handle;

struct eth_module {
  /** \brief Module's name. */
  char *name;

  /** \brief Module's environment (the thing implementing submodules). */
  eth_env *env;

  /** \brief Destructors to be called on deinitializatoin of a module. */
  cod_vec(closure) clos;

  /** \name Public variables.
   * \todo Use some dictionary.
   * @{ */
  int ndefs, defscap;
  eth_def *defs;
  /** @} */

  cod_vec(exit_handle) exithndls;

  /** \name Reference counting
   * \brief Reference counting to handle dpendency-relations between modules.
   * I.e., if module A imports B, then B must be kept alive, no matter what,
   * untill A is deinitialized. The reason (initial one, at least) is that if B
   * defines a new type, and A, e.g., defines a global variable of this type,
   * then destructor of that type (so the type-object itself) must be aveilable
   * during deinitialization of A. Thus deinitialization of modules must be done
   * in some complicated order respecting dependency-trees, which is implicitly
   * satisfied by reference ("dependency") counting.
   * @{ */
  //size_t rc;
  //cod_vec(eth_module*) deps;
  /** @} */

  // XXX: ugly workaround
  eth_root *memroot;
  char *mempath;
};

eth_module*
eth_create_module(const char *name, const char *dir)
{
  eth_module *mod = malloc(sizeof(eth_module));
  mod->name = name ? strdup(name) : strdup("<unnamed-module>");
  mod->ndefs = 0;
  mod->defscap = 0x10;
  mod->defs = malloc(sizeof(eth_def) * mod->defscap);
  mod->env = eth_create_empty_env();
  mod->memroot = NULL;
  mod->mempath = NULL;
  if (dir)
    eth_add_module_path(mod->env, dir);
  cod_vec_init(mod->clos);
  cod_vec_init(mod->exithndls);
  return mod;
}

void
eth_add_exit_handle(eth_module *mod, void (*cb)(void*), void *data)
{
  exit_handle hndle = { .cb = cb, .data = data };
  cod_vec_push(mod->exithndls, hndle);
}


void
eth_destroy_module(eth_module *mod)
{
  cod_vec_iter(mod->exithndls, i, x,
    eth_debug("exit handle: %p(%p)", x.cb, x.data);
    x.cb(x.data)
  );
  cod_vec_destroy(mod->exithndls);

  if (mod->mempath)
  {
    eth_debug("module '%s' was memorized, now forgeting it", mod->name);
    eth_forget_module(mod->memroot, mod->mempath);
    free(mod->mempath);
  }

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
  cod_vec_iter(mod->clos, i, x, if (x.dtor) x.dtor(x.data));
  cod_vec_destroy(mod->clos);
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
_eth_mark_memorized_module(eth_module *mod, eth_root *root, const char *path)
{
  assert(mod->memroot == NULL);
  assert(mod->mempath == NULL);
  mod->memroot = root;
  mod->mempath = strdup(path);
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
  eth_define_attr(mod, ident, val, eth_create_attr(0));
}

void
eth_copy_defs(const eth_module *src, eth_module *dst)
{
  for (int i = 0; i < src->ndefs; ++i)
  {
    const eth_def *def = src->defs + i;
    eth_define_attr(dst, def->ident, def->val, def->attr);
  }
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

void
eth_add_destructor(eth_module *mod, void *data, void (*dtor)(void*))
{
  closure c = { .data = data, .dtor = dtor };
  cod_vec_push(mod->clos, c);
}

