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
#include "codeine/hash-map.h"
#include "codeine/hash.h"

#include <dlfcn.h>


ETH_MODULE("ether:root")

#define hash cod_djb2


static void
destroy_module_entry(void *ptr)
{
  eth_module_entry *ent = ptr;
  eth_debug("calling DTOR for module '%s'", eth_get_module_name(ent->mod));
  eth_destroy_module(ent->mod);
  if (ent->dl)
    dlclose(ent->dl);
  free(ent);
}

struct eth_root {
  cod_hash_map *pathmap;
  eth_module *builtins;
  eth_env *env;
  int destroying; /* XXX hardcoded marker to avoid access to path-map while it
                     is being deinitialized */
};

eth_root*
eth_create_root(void)
{
  eth_root *root = malloc(sizeof(eth_root));
  root->pathmap = cod_hash_map_new(0);
  root->env = eth_create_env();
  root->destroying = false;
  root->builtins = eth_create_builtins(root);
  return root;
}

void
eth_destroy_root(eth_root *root)
{
  root->destroying = true;
  cod_hash_map_delete(root->pathmap, destroy_module_entry);
  eth_destroy_module(root->builtins);
  eth_destroy_env(root->env);
  free(root);
}

eth_env*
eth_get_root_env(eth_root *root)
{
  return root->env;
}

const eth_module*
eth_get_builtins(const eth_root *root)
{
  return root->builtins;
}

void
_eth_set_builtins(eth_root *root, eth_module *mod)
{
  root->builtins = mod;
}

eth_module_entry*
eth_get_memorized_module(const eth_root *root, const char *path)
{
  cod_hash_map_elt *elt = cod_hash_map_find(root->pathmap, path, hash(path));
  return elt ? elt->val : NULL;
}

eth_module_entry*
eth_memorize_module(eth_root *root, const char *path, eth_module *mod)
{
  extern void
  _eth_mark_memorized_module(eth_module *mod, eth_root *root, const char *path);

  // check path is already known
  if (eth_get_memorized_module(root, path))
  {
    eth_warning("module under \"%s\" is already in known", path);
    eth_warning("wont reassign it to module '%s'", eth_get_module_name(mod));
    return NULL;
  }

  // create new module-entry
  eth_module_entry *ent = malloc(sizeof(eth_module_entry));
  ent->mod = mod;
  ent->dl = NULL;
  ent->flag = 0;

  cod_hash_map_insert(root->pathmap, path, hash(path), (void*)ent, NULL);
  _eth_mark_memorized_module((eth_module*)mod, root, path);

  return ent;
}

void
eth_forget_module(eth_root *root, const char *path)
{
  if (root->destroying)
    return;

  if (not cod_hash_map_erase(root->pathmap, path, hash(path), free))
  {
    eth_warning("no module under \"%s\"", path);
    eth_warning("please ensure that the path abowe is a full path");
  }
}

eth_t
eth_get_builtin(eth_root *root, const char *name)
{
  eth_def *def = eth_find_def(root->builtins, name);
  if (def)
    return def->val;
  else
    return NULL;
}

