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


ETH_MODULE("ether:env")


#define hash cod_djb2

typedef struct {
  eth_module *mod;
  int flag;
} module_entry;

static void
destroy_module_entry(void *ptr)
{
  module_entry *ent = ptr;
  eth_destroy_module(ent->mod);
  free(ent);
}

struct eth_env {
  cod_vec(char*) modpath;
  cod_hash_map *mods;
};

eth_env*
eth_create_env(void)
{
  eth_env *env = malloc(sizeof(eth_env));
  cod_vec_init(env->modpath);
  env->mods = cod_hash_map_new();
  return env;
}

void
eth_destroy_env(eth_env *env)
{
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
    cod_vec_push(env->modpath, strdup(buf));
    return true;
  }
  else
  {
    eth_warning("invalid module-path");
    return false;
  }
}

bool
eth_resolve_path(eth_env *env, const char *path, char *fullpath)
{
  if (path[0] == '/')
    return realpath(path, fullpath);

  char buf[4102];
  for (size_t i = 0; i < env->modpath.len; ++i)
  {
    sprintf(buf, "%s/%s", env->modpath.data[i], path);
    if (realpath(buf, fullpath))
      return true;
  }
  return false;
}

static bool
add_module(eth_env *env, eth_module *mod, int flag)
{
  module_entry *ent = malloc(sizeof(module_entry));
  ent->mod = mod;
  ent->flag = flag;
  if (not cod_hash_map_insert(env->mods, mod->name, hash(mod->name), ent, NULL))
  {
    eth_warning("module %s already present", mod->name);
    destroy_module_entry(ent);
    return false;
  }
  return true;
}

bool
eth_add_module(eth_env *env, eth_module *mod)
{
  return add_module(env, mod, ETH_MFLAG_READY);
}

module_entry*
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

static eth_module*
load_from_script(eth_env *env, const char *path, const char *name)
{
  eth_debug("loading module %s", name);
  eth_module *mod = eth_create_module(name);
  add_module(env, mod, 0);

  char cwd[PATH_MAX];
  getcwd(cwd, PATH_MAX);

  FILE *file = fopen(path, "r");
  if (file == NULL)
  {
    eth_warning("failed to open file \"%s\"", path);
    return NULL;
  }

  char dirbuf[PATH_MAX];
  strcpy(dirbuf, path);
  char *dir = dirname(dirbuf);
  chdir(dir);

  eth_ast *ast = eth_parse(file);
  fclose(file);
  if (ast == NULL)
  {
    eth_warning("can't load module %s", name);
    chdir(cwd);
    return NULL;
  }

  eth_ir_defs defs;
  eth_ir *ir = eth_build_ir(ast, env, &defs);
  eth_drop_ast(ast);
  if (ir == NULL)
  {
    eth_warning("can't load module %s", name);
    chdir(cwd);
    return NULL;
  }

  eth_ssa *ssa = eth_build_ssa(ir, &defs);
  eth_drop_ir(ir);
  eth_destroy_ir_defs(&defs);
  if (ssa == NULL)
  {
    eth_warning("can't load module %s", name);
    chdir(cwd);
    return NULL;
  }

  eth_bytecode *bc = eth_build_bytecode(ssa);
  eth_destroy_ssa(ssa);
  if (bc == NULL)
  {
    eth_warning("can't load module %s", name);
    chdir(cwd);
    return NULL;
  }

  eth_t ret = eth_vm(bc);
  eth_ref(ret);
  eth_drop_bytecode(bc);
  if (ret->type == eth_exception_type)
  {
    eth_warning("failed to load module %s due to exception (~w)", name, ret);
    eth_unref(ret);
    chdir(cwd);
    return NULL;
  }

  chdir(cwd);

  for (eth_t it = ret; it != eth_nil; it = eth_cdr(it))
  {
    eth_t x = eth_car(it);
    char *ident = ETH_STRING(eth_car(x))->cstr;
    eth_t value = eth_cdr(x);
    eth_define(mod, ident, value);
  }
  eth_unref(ret);

  eth_debug("module %s was succesfully loaded", name);
  module_entry *ent = get_module_entry(env, name);
  assert(ent);
  ent->flag |= ETH_MFLAG_READY;
  return ent->mod;
}

eth_module*
eth_load_module_from_script(eth_env *env, const char *path, const char *name)
{
  char buf[PATH_MAX];
  if (name == NULL)
  {
    strcpy(buf, path);
    name = basename(buf);
    ((char*)name)[0] = toupper(name[0]);
    char *p = strrchr(name, '.');
    if (p) *p = 0;
  }

  if (get_module_entry(env, name))
  {
    eth_warning("module with name %s already present");
    return NULL;
  }
  return load_from_script(env, path, name);
}

mode_t
get_file_mode(const char *path)
{
  struct stat path_stat;
  stat(path, &path_stat);
  return S_ISREG(path_stat.st_mode);
}

static bool
resolve_path_by_name(eth_env *env, const char *name, char *path)
{
  char buf[4101]; // to suppress gcc warning
  if (eth_resolve_path(env, name, path))
  {
    mode_t mode = get_file_mode(path);
    if (S_ISDIR(mode))
    {
      sprintf(buf, "%s/%s.eth", path, name);
      if (realpath(buf, path))
        return true;
    }
  }

  sprintf(buf, "%s.eth", name);
  if (eth_resolve_path(env, buf, path))
    return true;

  return false;
}

eth_module*
eth_require_module(eth_env *env, const char *name)
{
  char path[PATH_MAX];
  char lowname[strlen(name) + 1];

  module_entry *ent;
  if ((ent = get_module_entry(env, name)))
  {
    if (ent->flag & ETH_MFLAG_READY)
      return ent->mod;
    else
    {
      eth_warning("mode %s required while still loading", name);
      return false;
    }
  }

  if (resolve_path_by_name(env, name, path))
    goto load_module;

  // try lowercase
  for (size_t i = 0; i < sizeof lowname; ++i)
    lowname[i] = tolower(name[i]);
  if (resolve_path_by_name(env, lowname, path))
    goto load_module;

  eth_debug("failed to resolve module path:");
  eth_debug("  module name = %s", name);
  eth_debug("  module path:");
  for (size_t i = 0; i < env->modpath.len; ++i)
    eth_debug("  - %s", env->modpath.data[i]);

  return false;

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
load_module:
  return load_from_script(env, path, name);
}
