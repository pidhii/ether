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
#include "codeine/vec.h"

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>


ETH_MODULE("ether:env")


#define hash cod_djb2

typedef struct {
  eth_module *mod;
  void *dl;
  int flag;
} module_entry;

static void
destroy_module_entry(void *ptr)
{
  module_entry *ent = ptr;
  eth_debug("calling DTOR for module '%s'", eth_get_module_name(ent->mod));
  eth_destroy_module(ent->mod);
  if (ent->dl)
    dlclose(ent->dl);
  free(ent);
}

typedef struct {
  void (*cb)(void*);
  void *data;
} exit_handle;

struct eth_env {
  cod_vec(char*) modpath;
  cod_vec(exit_handle) exithndls;
  cod_hash_map *mods;
};

eth_env*
eth_create_empty_env(void)
{
  eth_env *env = malloc(sizeof(eth_env));
  cod_vec_init(env->modpath);
  cod_vec_init(env->exithndls);
  env->mods = cod_hash_map_new();
  return env;
}

eth_env*
eth_create_env(void)
{
  eth_env *env = eth_create_empty_env();
  /*eth_add_module_path(env, ".");*/
  if (eth_get_prefix())
  {
    char path[PATH_MAX];
    sprintf(path, "%s/lib/ether", eth_get_prefix());
    eth_add_module_path(env, path);
  }
  return env;
}

void
eth_destroy_env(eth_env *env)
{
  cod_vec_iter(env->exithndls, i, x, eth_debug("exit handle: %p(%p)", x.cb, x.data); x.cb(x.data));
  cod_vec_destroy(env->exithndls);
  cod_vec_iter(env->modpath, i, x, free(x));
  cod_vec_destroy(env->modpath);
  cod_hash_map_delete(env->mods, destroy_module_entry);
  free(env);
}

bool
eth_add_module_path(eth_env *env, const char *path)
{
  char buf[PATH_MAX];
  if (realpath(path, buf))
  {
    cod_vec_push(env->modpath, strdup(path));
    return true;
  }
  else
  {
    eth_warning("invalid module-path, \"%s\"", path);
    return false;
  }
}

void
eth_add_exit_handle(eth_env *env, void (*cb)(void*), void *data)
{
  exit_handle hndle = { .cb = cb, .data = data };
  cod_vec_push(env->exithndls, hndle);
}

bool
eth_resolve_path(eth_env *env, const char *path, char *fullpath)
{
  eth_debug("resolving path \"%s\"", path);
  if (path[0] == '/')
    return realpath(path, fullpath);

  char buf[4102];
  for (size_t i = 0; i < env->modpath.len; ++i)
  {
    sprintf(buf, "%s/%s", env->modpath.data[i], path);
    eth_debug("[\"%s\"] check \"%s\"", path, buf);
    if (realpath(buf, fullpath))
      return true;
  }
  return false;
}

static module_entry*
add_module(eth_env *env, eth_module *mod, int flag)
{
  const char *modname = eth_get_module_name(mod);
  module_entry *ent = malloc(sizeof(module_entry));
  ent->mod = mod;
  ent->dl = NULL;
  ent->flag = flag;
  if (not cod_hash_map_insert(env->mods, modname, hash(modname), ent, NULL))
  {
    eth_warning("module %s already present", modname);
    destroy_module_entry(ent);
    return NULL;
  }
  return ent;
}

static module_entry*
add_dl_module(eth_env *env, eth_module *mod, void* dl,  int flag)
{
  module_entry *ent = add_module(env, mod, flag);
  if (not ent) return NULL;
  ent->dl = dl;
  return ent;
}

bool
eth_remove_module(eth_env *env, const char *name)
{
  return cod_hash_map_erase(env->mods, name, hash(name), free);
}

bool
eth_add_module(eth_env *env, eth_module *mod)
{
  return add_module(env, mod, ETH_MFLAG_READY);
}

static module_entry*
get_module_entry(const eth_env *env, const char *name)
{
  cod_hash_map_elt* elt = cod_hash_map_find(env->mods, name, hash(name));
  return elt ? elt->val : NULL;
}

int
eth_get_nmodules(const eth_env *env)
{
  return env->mods->size;
}

void
eth_get_modules(const eth_env *env, const eth_module *out[], int n)
{
  cod_hash_map_iter iter;
  cod_hash_map_begin(env->mods, &iter);
  module_entry *ent;
  for (int i = 0; cod_hash_map_next(env->mods, NULL, &ent, &iter); ++i)
    out[i] = ent->mod;
}

static bool
load_from_ast(eth_env *topenv, eth_module *mod, eth_ast *ast,
    eth_t *retptr, eth_module *uservars)
{
  eth_ir_defs defs;
  eth_ir *ir = eth_build_module_ir(ast, topenv, mod, &defs, uservars);
  if (ir == NULL)
    goto error;

  eth_ssa *ssa = eth_build_ssa(ir, &defs);
  eth_drop_ir(ir);
  if (ssa == NULL)
  {
    eth_destroy_ir_defs(&defs);
    goto error;
  }

  eth_bytecode *bc = eth_build_bytecode(ssa);
  eth_drop_ssa(ssa);
  if (bc == NULL)
  {
    eth_destroy_ir_defs(&defs);
    goto error;
  }

  eth_t ret = eth_vm(bc);
  eth_ref(ret);
  eth_drop_bytecode(bc);
  if (ret->type == eth_exception_type)
  {
    eth_destroy_ir_defs(&defs);
    eth_warning("failed to load module %s (~w)", eth_get_module_name(mod), ret);
    eth_unref(ret);
    return false;
  }

  // get defs:
  int i = 0;
  for (eth_t it = eth_tup_get(ret, 1); it != eth_nil; it = eth_cdr(it), ++i)
    eth_define_attr(mod, defs.idents[i], eth_car(it), defs.attrs[i]);
  eth_destroy_ir_defs(&defs);
  // get return-value:
  if (retptr)
  {
    *retptr = eth_tup_get(ret, 0);
    eth_ref(*retptr);
    eth_unref(ret);
    eth_dec(*retptr);
  }
  else
    eth_unref(ret);

  eth_debug("module %s was succesfully loaded", eth_get_module_name(mod));
  return true;

error:
  eth_warning("can't load module %s", eth_get_module_name(mod));
  return false;
}

bool
eth_load_module_from_ast2(eth_env *topenv, eth_env *env, eth_module *mod,
    eth_ast *ast, eth_t *ret, eth_module *uservars)
{
  bool status = true;

  // create temporary environment if one is not supplied
  bool isnullenv = env == NULL;
  if (env == NULL)
    env = eth_create_empty_env();

  module_entry *ent = get_module_entry(env, eth_get_module_name(mod));
  if (ent)
  {
    if (ent->mod == mod)
      eth_debug("updating module '%s'", eth_get_module_name(mod));
    else
    {
      eth_warning("module with name '%s' already present", eth_get_module_name(mod));
      status = false;
      goto error;
    }
  }
  else
    eth_debug("loading script-module %s", eth_get_module_name(mod));

  if (ent)
    ; // will update existing module
  else
    ent = add_module(env, mod, 0);

  int ok = load_from_ast(topenv, mod, ast, ret, uservars);
  ent->flag |= ETH_MFLAG_READY;
  if (not ok)
  {
    eth_remove_module(env, eth_get_module_name(mod));
    status = false;
    goto error;
  }

error:
  if (isnullenv)
  {
    eth_remove_module(env, eth_get_module_name(mod));
    eth_destroy_env(env);
  }
  return status;
}

bool
eth_load_module_from_script2(eth_env *topenv, eth_env *env, eth_module *mod,
    const char *path, eth_t *ret, eth_module *uservars)
{
  bool status = true;

  char fullpath[PATH_MAX];
  if (not realpath(path, fullpath))
  {
    eth_warning("no such file \"%s\"", path);
    return false;
  }

  FILE *file = fopen(path, "r");
  if (file == NULL)
    return false;

  eth_ast *ast = eth_parse(file);
  fclose(file);
  if (ast == NULL)
    return false;

  char cwd[PATH_MAX];
  getcwd(cwd, PATH_MAX);

  char dirbuf[PATH_MAX];
  strcpy(dirbuf, fullpath);
  char *dir = dirname(dirbuf);

  chdir(dir);
  bool ok = eth_load_module_from_ast2(topenv, env, mod, ast, ret, uservars);
  eth_drop_ast(ast);
  chdir(cwd);
  return true;
}

bool
eth_load_module_from_ast(eth_env *topenv, eth_env *env, eth_module *mod,
    eth_ast *ast, eth_t *ret)
{
  return eth_load_module_from_ast2(topenv, env, mod, ast, ret, NULL);
}

bool
eth_load_module_from_script(eth_env *topenv, eth_env *env, eth_module *mod,
    const char *path, eth_t *ret)
{
  return eth_load_module_from_script2(topenv, env, mod, path, ret, NULL);
}

void*
load_from_elf(eth_env *topenv, eth_env *env, eth_module *mod, const char *path)
{
  eth_debug("loading ELF-module %s", eth_get_module_name(mod));

  void *dl = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  if (not dl)
  {
    eth_warning("%s", dlerror());
    return NULL;
  }

  int (*init)(eth_module*, eth_env *topenv) = dlsym(dl, "ether_module");
  if (not init)
  {
    eth_warning("%s", dlerror());
    dlclose(dl);
    return NULL;
  }

  if (init(mod, topenv))
  {
    eth_warning("failed to load module %s", eth_get_module_name(mod));
    dlclose(dl);
    return NULL;
  }

  eth_debug("module %s was succesfully loaded", eth_get_module_name(mod));
  return dl;
}

// TODO: code duplication (see eth_load_module_from_script())
bool
eth_load_module_from_elf(eth_env *topenv, eth_env *env, eth_module *mod,
    const char *path)
{
  char fullpath[PATH_MAX];
  if (not realpath(path, fullpath))
  {
    eth_warning("no such file \"%s\"", path);
    return false;
  }

  if (get_module_entry(env, eth_get_module_name(mod)))
  {
    eth_warning("module with name '%s' already present", eth_get_module_name(mod));
    return false;
  }

  char cwd[PATH_MAX];
  getcwd(cwd, PATH_MAX);

  char dirbuf[PATH_MAX];
  strcpy(dirbuf, fullpath);
  char *dir = dirname(dirbuf);

  chdir(dir);
  module_entry *ent = add_module(env, mod, 0);
  void *dl = load_from_elf(topenv, env, mod, fullpath);
  ent->flag |= ETH_MFLAG_READY;
  chdir(cwd);

  if (not dl)
  {
    eth_remove_module(env, eth_get_module_name(mod));
    return false;
  }

  ent->dl = dl;
  return true;
}

mode_t
get_file_mode(const char *path)
{
  struct stat path_stat;
  stat(path, &path_stat);
  return path_stat.st_mode;
}

static bool
resolve_path_by_name(eth_env *env, const char *name, char *path)
{
  eth_debug("searching for '%s'", name);

  static char buf[4109]; // to suppress gcc warning
  eth_debug("[%s] check \"%s\"", name, name);
  if (eth_resolve_path(env, name, path))
  {
    mode_t mode = get_file_mode(path);
    if (S_ISREG(mode))
    {
      eth_debug("[%s] found file \"%s\"", name, path);
      return true;
    }

    if (S_ISDIR(mode))
    {
      eth_debug("[%s] found directory \"%s\"", name, path);

      char dir[PATH_MAX];
      strcpy(dir, path);

      sprintf(buf, "%s/__main__.eth", dir);
      eth_debug("[%s] check \"%s\"", name, buf);
      if (realpath(buf, path))
        return true;

      sprintf(buf, "%s/__main__.so", dir);
      eth_debug("[%s] check \"%s\"", name, buf);
      if (realpath(buf, path))
        return true;

      sprintf(buf, "%s/%s.eth", dir, name);
      eth_debug("[%s] check \"%s\"", name, buf);
      if (realpath(buf, path))
        return true;

      sprintf(buf, "%s/%s.so", dir, name);
      eth_debug("[%s] check \"%s\"", name, buf);
      if (realpath(buf, path))
        return true;
    }
  }

  sprintf(buf, "%s.eth", name);
  eth_debug("[%s] check \"%s\"", name, buf);
  if (eth_resolve_path(env, buf, path))
    return true;

  sprintf(buf, "%s.so", name);
  eth_debug("[%s] check \"%s\"", name, buf);
  if (eth_resolve_path(env, buf, path))
    return true;

  eth_debug("[%s] serach failed", name);
  return false;
}

static bool
is_elf(const char *path)
{
  static char elf[] = { 0x7f, 'E', 'L', 'F' };
  char buf[4] = { 0, 0, 0, 0 };

  FILE *f = fopen(path, "rb");
  assert(f);
  fread(buf, 1, 4, f);
  bool ret = memcmp(buf, elf, 4) == 0;
  fclose(f);
  return ret;
}

static eth_module*
find_module_in_env(eth_env *env, const char *name, bool *e)
{
  char *p;
  if ((p = strchr(name, '.')))
  {
    int len = p - name;
    char topname[len + 1];
    memcpy(topname, name, len);
    topname[len] = 0;

    module_entry *ent;
    if ((ent = get_module_entry(env, topname)))
      return find_module_in_env(eth_get_env(ent->mod), p + 1, e);
    else
      return NULL;
  }
  else
  {
    module_entry *ent;
    if ((ent = get_module_entry(env, name)))
    {
      if (ent->flag & ETH_MFLAG_READY)
        return ent->mod;
      else
      {
        eth_warning("module %s required while still loading", name);
        *e = 1;
        return NULL;
      }
    }
    else
      return NULL;
  }
}

static eth_module*
load_module(eth_env *topenv, eth_env *env, const char *name)
{
  char path[PATH_MAX];
  int namelen = strlen(name);
  int topnamelen;
  char topname[namelen + 1];

  char *p;
  if ((p = strchr(name, '.')))
  {
    topnamelen = p - name;
    memcpy(topname, name, topnamelen);
    topname[topnamelen] = 0;
  }
  else
  {
    topnamelen = namelen;
    memcpy(topname, name, namelen + 1);
  }

  if (not resolve_path_by_name(env, topname, path))
  {
    // try lowercase
    char lowname[topnamelen + 1];
    for (int i = 0; i < topnamelen; ++i)
      lowname[i] = tolower(topname[i]);
    lowname[topnamelen] = 0;
    // ---
    if (not resolve_path_by_name(env, lowname, path))
    {
      eth_debug("can't find module %s", topname);
      eth_debug("module path:");
      for (size_t i = 0; i < env->modpath.len; ++i)
        eth_debug("  - %s", env->modpath.data[i]);
      return NULL;
    }
  }

  eth_module *mod = eth_create_module(topname);
  bool ok;
  if (is_elf(path))
    ok = eth_load_module_from_elf(topenv, env, mod, path);
  else
    ok = eth_load_module_from_script(topenv, env, mod, path, NULL);

  if (not ok)
  {
    eth_destroy_module(mod);
    return NULL;
  }
  return mod;
}

eth_module*
eth_require_module(eth_env *topenv, eth_env *env, const char *name)
{
  eth_module *mod;
  char path[PATH_MAX];
  char lowname[strlen(name) + 1];

  // search available modules
  bool e = false;
  if ((mod = find_module_in_env(env, name, &e)))
    return mod;
  else if (e)
    return NULL;
  else
    return load_module(topenv, env, name);
}

