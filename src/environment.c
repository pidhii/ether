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


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                                ENVIRONMENT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct eth_env {
  cod_vec(char*) modpath;
};

eth_env*
eth_create_empty_env(void)
{
  eth_env *env = malloc(sizeof(eth_env));
  cod_vec_init(env->modpath);
  return env;
}

eth_env*
eth_create_env(void)
{
  eth_env *env = eth_create_empty_env();
  /*eth_add_module_path(env, ".");*/
  if (eth_get_module_path())
    eth_add_module_path(env, eth_get_module_path());
  return env;
}

void
eth_destroy_env(eth_env *env)
{
  cod_vec_iter(env->modpath, i, x, free(x));
  cod_vec_destroy(env->modpath);
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
eth_copy_module_path(const eth_env *src, eth_env *dst)
{
  for (size_t i = 0; i < src->modpath.len; ++i)
    eth_add_module_path(dst, src->modpath.data[i]);
}

bool
eth_resolve_path(eth_env *env, const char *path, char *fullpath)
{
  eth_debug("resolving (relative) path \"%s\": ", path);
  eth_indent_log();

  if (path[0] == '/' or strncmp(path, "./", 2) == 0)
  {
    eth_debug("realpath? \"%s\" -> ", path, false);
    if (realpath(path, fullpath))
    {
      if (eth_debug_enabled)
        fprintf(stderr, "\e[38;5;2;1myes\e[0m\n");
      eth_dedent_log();
      return true;
    }
    else
    {
      if (eth_debug_enabled)
        fprintf(stderr, "\e[38;5;1;1mno\e[0m\n");
      eth_dedent_log();
      return false;
    }
  }

  char buf[4102];
  for (size_t i = 0; i < env->modpath.len; ++i)
  {
    sprintf(buf, "%s/%s", env->modpath.data[i], path);
    eth_debug("realpath? \"%s\" -> ", buf, false);
    if (realpath(buf, fullpath))
    {
      if (eth_debug_enabled)
        fprintf(stderr, "\e[38;5;2;1myes\e[0m\n");
      eth_dedent_log();
      return true;
    }
    else if (eth_debug_enabled)
      fprintf(stderr, "\e[38;5;1;1mno\e[0m\n");
  }
  eth_dedent_log();
  return false;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                       Generic AST handeling
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static bool
load_from_ast(eth_root *root, eth_module *mod, eth_ast *ast,
    eth_t *retptr, eth_module *uservars)
{
  eth_debug("building module '%s'", eth_get_module_name(mod));
  eth_indent_log();

  eth_debug("building IR...");
  eth_indent_log();
  eth_ir_defs defs;
  eth_ir *ir = eth_build_module_ir(ast, root, mod, &defs, uservars);
  if (ir == NULL)
    goto error;
  eth_dedent_log();

  eth_debug("building SSA...");
  eth_indent_log();
  eth_ssa *ssa = eth_build_ssa(ir, &defs);
  eth_drop_ir(ir);
  if (ssa == NULL)
  {
    eth_destroy_ir_defs(&defs);
    goto error;
  }
  eth_dedent_log();

  eth_debug("converting SSA to bytecode...");
  eth_indent_log();
  eth_bytecode *bc = eth_build_bytecode(ssa);
  eth_drop_ssa(ssa);
  if (bc == NULL)
  {
    eth_destroy_ir_defs(&defs);
    goto error;
  }
  eth_dedent_log();

  eth_debug("evaluating module...");
  eth_indent_log();
  eth_t ret = eth_vm(bc);
  eth_dedent_log();
  eth_dedent_log();

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
    eth_define_attr(mod, defs.defs[i].ident, eth_car(it), defs.defs[i].attr);
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
  eth_dedent_log();
  eth_dedent_log();
  eth_warning("can't load module %s", eth_get_module_name(mod));
  return false;
}

bool
eth_load_module_from_ast(eth_root *root, eth_module *mod, eth_ast *ast,
    eth_t *ret)
{
  return eth_load_module_from_ast2(root, mod, ast, ret, NULL);
}

bool
eth_load_module_from_ast2(eth_root *root, eth_module *mod, eth_ast *ast,
    eth_t *ret, eth_module *uservars)
{
  return load_from_ast(root, mod, ast, ret, uservars);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                           Script handeling
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/*
 *  <module-ptr>       -> OK
 *  NULL               -> not available
 * (eth_module_entry*)(-1) -> cyclic dependence
 */
static eth_module*
check_memorized(const eth_root *root, const char *path)
{
  eth_module_entry *ent = eth_get_memorized_module(root, path);
  if (ent)
  {
    if (ent->flag & ETH_MFLAG_READY)
    {
      eth_debug("found module \"%s\" in memory", path);
      return ent->mod;
    }
    else
    {
      eth_warning("cyclic dependence detected while requiering module \"%s\"",
          path);
      return (eth_module*)-1;
    }
  }
  else
    return NULL;
}

// XXX: must check if module was already loaded
eth_module*
eth_load_module_from_script2(eth_root *root, const char *path, eth_t *ret,
    eth_module *uservars)
{
  char fullpath[PATH_MAX];
  if (not realpath(path, fullpath))
  {
    eth_warning("no such file \"%s\"", path);
    return NULL;
  }

  eth_module *memmod;
  if ((memmod = check_memorized(root, fullpath)))
  {
    if (memmod == (eth_module*)(-1))
      return NULL;
    else
      return memmod;
  }

  FILE *file = fopen(path, "r");
  if (file == NULL)
    return NULL;

  eth_ast *ast = eth_parse(root, file);
  fclose(file);
  if (ast == NULL)
    return NULL;

  char cwd[PATH_MAX];
  getcwd(cwd, PATH_MAX);

  char dirbuf[PATH_MAX];
  strcpy(dirbuf, fullpath);
  char *dir = dirname(dirbuf);

  chdir(dir);
  eth_module *mod = eth_create_module(fullpath, dir);
  eth_copy_module_path(eth_get_root_env(root), eth_get_env(mod));
  eth_module_entry *ent = eth_memorize_module(root, fullpath, mod);
  bool ok = eth_load_module_from_ast2(root, mod, ast, ret, uservars);
  eth_drop_ast(ast);
  chdir(cwd);

  if (not ok)
  {
    eth_forget_module(root, fullpath);
    return NULL;
  }
  else
  {
    ent->flag |= ETH_MFLAG_READY;
    return mod;
  }
}

eth_module*
eth_load_module_from_script(eth_root *root, const char *path, eth_t *ret)
{
  return eth_load_module_from_script2(root, path, ret, NULL);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                              ELF handeling
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
void*
load_from_elf(eth_root *root, eth_module *mod, const char *path)
{
  eth_debug("loading ELF-module %s", eth_get_module_name(mod));

  void *dl = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  if (not dl)
  {
    eth_warning("%s", dlerror());
    return NULL;
  }

  // XXX updated signature
  int (*init)(eth_module*, eth_root*) = dlsym(dl, "ether_module");
  if (not init)
  {
    eth_warning("%s", dlerror());
    dlclose(dl);
    return NULL;
  }

  if (init(mod, root))
  {
    eth_warning("failed to load module %s", eth_get_module_name(mod));
    dlclose(dl);
    return NULL;
  }

  eth_debug("module %s was succesfully loaded", eth_get_module_name(mod));
  return dl;
}

// XXX: must check if module was already loaded
eth_module*
eth_load_module_from_elf(eth_root *root, const char *path)
{
  char fullpath[PATH_MAX];
  if (not realpath(path, fullpath))
  {
    eth_warning("no such file \"%s\"", path);
    return false;
  }

  eth_module *memmod;
  if ((memmod = check_memorized(root, fullpath)))
  {
    if (memmod == (eth_module*)(-1))
      return false;
    else
      return memmod;
  }

  char cwd[PATH_MAX];
  getcwd(cwd, PATH_MAX);

  char dirbuf[PATH_MAX];
  strcpy(dirbuf, fullpath);
  char *dir = dirname(dirbuf);

  chdir(dir);
  eth_module *mod = eth_create_module(fullpath, dir);
  eth_module_entry *ent = eth_memorize_module(root, fullpath, mod);
  void *dl = load_from_elf(root, mod, fullpath);
  ent->dl = dl;
  chdir(cwd);

  if (not dl)
  {
    eth_forget_module(root, path);
    return NULL;
  }
  else
  {
    ent->flag |= ETH_MFLAG_READY;
    return mod;
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                            Generic load
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static mode_t
get_file_mode(const char *path)
{
  struct stat path_stat;
  stat(path, &path_stat);
  return path_stat.st_mode;
}

bool
eth_resolve_modpath(eth_env *env, const char *modpath, char *fullpath)
{
  eth_debug("searching for '%s'", modpath);
  eth_indent_log();

  static char buf[4109]; // to suppress gcc warning
  /*eth_debug("check \"%s\"", modpath);*/
  if (eth_resolve_path(env, modpath, fullpath))
  {
    mode_t mode = get_file_mode(fullpath);
    if (S_ISREG(mode))
    {
      eth_debug("found file \"%s\"", fullpath);
      eth_dedent_log();
      eth_debug("found sources \e[38;5;2;1m✓\e[0m");
      return true;
    }

    if (S_ISDIR(mode))
    {
      eth_debug("found directory \"%s\"", fullpath);

      char dir[PATH_MAX];
      strcpy(dir, fullpath);

      sprintf(buf, "%s/__main__.eth", dir);
      eth_debug("realpath? \"%s\" -> ", buf, false);
      if (realpath(buf, fullpath))
        goto found;
      if (eth_debug_enabled)
        fprintf(stderr, "\e[38;5;1;1mno\e[0m\n");

      sprintf(buf, "%s/__main__.so", dir);
      eth_debug("realpath? \"%s\" -> ", buf, false);
      if (realpath(buf, fullpath))
        goto found;
      if (eth_debug_enabled)
        fprintf(stderr, "\e[38;5;1;1mno\e[0m\n");

      sprintf(buf, "%s/%s.eth", dir, modpath);
      eth_debug("realpath? \"%s\" -> ", buf, false);
      if (realpath(buf, fullpath))
        goto found;
      if (eth_debug_enabled)
        fprintf(stderr, "\e[38;5;1;1mno\e[0m\n");

      sprintf(buf, "%s/%s.so", dir, modpath);
      eth_debug("realpath? \"%s\" -> ", buf, false);
      if (realpath(buf, fullpath))
        goto found;
      if (eth_debug_enabled)
        fprintf(stderr, "\e[38;5;1;1mno\e[0m\n");
    }
  }

  sprintf(buf, "%s.eth", modpath);
  eth_debug("check \"%s\"", buf);
  if (eth_resolve_path(env, buf, fullpath))
  {
    eth_dedent_log();
    eth_debug("found sources \e[38;5;2;1m✓\e[0m");
    return true;
  }

  sprintf(buf, "%s.so", modpath);
  eth_debug("check \"%s\"", buf);
  if (eth_resolve_path(env, buf, fullpath))
  {
    eth_dedent_log();
    eth_debug("found sources \e[38;5;2;1m✓\e[0m");
    return true;
  }

  eth_dedent_log();
  eth_debug("search failed \e[38;5;1;1m✗\e[0m", modpath);
  return false;

found:
  if (eth_debug_enabled)
    fprintf(stderr, "\e[38;5;2;1myes\e[0m\n");
  eth_dedent_log();
  return true;

return_false:
  eth_dedent_log();
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
load_module(eth_root *root, eth_env *env, const char *modpath)
{
  char fullpath[PATH_MAX], dir[PATH_MAX];

  if (not eth_resolve_modpath(env, modpath, fullpath))
  {
    eth_debug("can't find module %s", modpath);
    eth_debug("module path:");
    for (size_t i = 0; i < env->modpath.len; ++i)
      eth_debug("  - %s", env->modpath.data[i]);
    return NULL;
  }

  eth_module *mod;
  if (is_elf(fullpath))
    mod = eth_load_module_from_elf(root, fullpath);
  else
    mod = eth_load_module_from_script(root, fullpath, NULL);

  return mod;
}

eth_module*
eth_require_module(eth_root *root, eth_env *env, const char *modpath)
{
  return load_module(root, env, modpath);
}

