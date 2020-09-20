#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

static eth_t
_chdir(void)
{
  eth_use_variant(System_error)

  eth_args args = eth_start(1);
  eth_t path = eth_arg2(args, eth_string_type);
  if (chdir(eth_str_cstr(path)))
  {
    eth_t err = eth_sym(eth_errno_to_str(errno));
    eth_throw(args, System_error(err));
  }
  eth_return(args, eth_nil);
}

static eth_t
_access(void)
{
  eth_use_variant(System_error)

  eth_args args = eth_start(2);
  eth_t path = eth_arg2(args, eth_string_type);
  eth_t amode = eth_arg2(args, eth_number_type);
  if (access(eth_str_cstr(path), eth_num_val(amode)))
  {
    if (errno == EACCES)
      eth_return(args, eth_false);
    else
    {
      eth_t err = eth_sym(eth_errno_to_str(errno));
      eth_throw(args, System_error(err));
    }
  }
  eth_return(args, eth_true);
}

static eth_t
_getcwd(void)
{
  eth_use_variant(System_error)

  char buf[PATH_MAX];
  if (not getcwd(buf, PATH_MAX))
  {
    eth_t err = eth_sym(eth_errno_to_str(errno));
    return System_error(err);
  }
  return eth_str(buf);
}

static eth_t
_realpath(void)
{
  eth_use_variant(System_error)

  eth_args args = eth_start(1);
  eth_t path = eth_arg2(args, eth_string_type);
  char buf[PATH_MAX];
  if (not realpath(eth_str_cstr(path), buf))
  {
    eth_t err = eth_sym(eth_errno_to_str(errno));
    eth_throw(args, System_error(err));
  }
  eth_return(args, eth_str(buf));
}

int
ether_module(eth_module *mod, eth_env *topenv)
{
  eth_define(mod, "__chdir", eth_create_proc(_chdir, 1, NULL, NULL));

  eth_define(mod, "f_ok", eth_num(F_OK));
  eth_define(mod, "r_ok", eth_num(R_OK));
  eth_define(mod, "w_ok", eth_num(W_OK));
  eth_define(mod, "x_ok", eth_num(X_OK));
  eth_define(mod, "__access", eth_create_proc(_access, 2, NULL, NULL));
  eth_define(mod, "__getcwd", eth_create_proc(_getcwd, 0, NULL, NULL));
  eth_define(mod, "__realpath", eth_create_proc(_realpath, 1, NULL, NULL));

  int ret = 0;
  if (not eth_load_module_from_script2(topenv, NULL, mod, "./lib.eth", NULL, mod))
    ret = -1;
  return ret;
}

