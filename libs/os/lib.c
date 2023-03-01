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
#define _GNU_SOURCE
#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

static eth_t
_chdir(void)
{
  eth_use_variant(system_error)

  eth_args args = eth_start(1);
  eth_t path = eth_arg2(args, eth_string_type);
  if (chdir(eth_str_cstr(path)))
  {
    eth_t err = eth_sym(eth_errno_to_str(errno));
    eth_throw(args, system_error(err));
  }
  eth_return(args, eth_nil);
}

static eth_t
_access(void)
{
  eth_use_variant(system_error)

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
      eth_throw(args, system_error(err));
    }
  }
  eth_return(args, eth_true);
}

static eth_t
_getenv(void)
{
  eth_args args = eth_start(1);
  eth_t name = eth_arg2(args, eth_string_type);
  const char *val = getenv(eth_str_cstr(name));
  if (val)
    eth_return(args, eth_str(val));
  else
    eth_throw(args, eth_failure());
}

static eth_t
_setenv(void)
{
  eth_args args = eth_start(3);
  eth_t name = eth_arg2(args, eth_string_type);
  eth_t value = eth_arg2(args, eth_string_type);
  eth_t overwrite = eth_arg(args);
  errno = 0;
  if (setenv(eth_str_cstr(name), eth_str_cstr(value), overwrite != eth_false))
    eth_throw(args, eth_system_error(errno));
  eth_return(args, eth_nil);
}

static eth_t
_unsetenv(void)
{
  eth_args args = eth_start(1);
  eth_t name = eth_arg2(args, eth_string_type);
  errno = 0;
  if (unsetenv(eth_str_cstr(name)))
    eth_throw(args, eth_system_error(errno));
  eth_return(args, eth_nil);
}

static eth_t
_getcwd(void)
{
  eth_use_variant(system_error)

  char buf[PATH_MAX];
  if (not getcwd(buf, PATH_MAX))
  {
    eth_t err = eth_sym(eth_errno_to_str(errno));
    return eth_exn(system_error(err));
  }
  return eth_str(buf);
}

static eth_t
_realpath(void)
{
  eth_use_variant(system_error)

  eth_args args = eth_start(1);
  eth_t path = eth_arg2(args, eth_string_type);
  char buf[PATH_MAX];
  if (not realpath(eth_str_cstr(path), buf))
    eth_throw(args, eth_system_error(errno));
  eth_return(args, eth_str(buf));
}

static eth_t
_mkdtemp(void)
{
  eth_args args = eth_start(1);
  eth_t temp = eth_arg2(args, eth_string_type);
  char buf[eth_str_len(temp)+1];
  strcpy(buf, eth_str_cstr(temp));
  if (not mkdtemp(buf))
    eth_throw(args, eth_str(eth_errno_to_str(errno)));
  else
    eth_return(args, eth_str(buf));
}

static eth_t
_fork(void)
{
  int pid = fork();
  return pid < 0 ? eth_exn(eth_system_error(errno)) : eth_num(pid);
}

static eth_t
_waitpid(void)
{
  eth_args args = eth_start(2);
  eth_t pid = eth_arg2(args, eth_number_type);
  eth_t options = eth_arg2(args, eth_number_type);
  int wstatus = 0;

  int ret = waitpid(eth_num_val(pid), &wstatus, (int)eth_num_val(options));
  int err = errno;
  if (ret < 0)
    eth_throw(args, eth_system_error(err));
  eth_return(args, eth_tup2(eth_num(ret), eth_num(wstatus)));
}

static eth_t
_pipe(void)
{
  eth_use_variant(system_error);

  int fildes[2];
  if (pipe(fildes) < 0)
  {
    return eth_exn(system_error(eth_str(eth_errno_to_str(errno))));
  }
  else
  {
    eth_t rx = eth_open_fd(fildes[0], "r");
    eth_t tx = eth_open_fd(fildes[1], "w");
    return eth_tup2(rx, tx);
  }
}

static eth_t
_pipe2(void)
{
  eth_use_variant(system_error);

  eth_args args = eth_start(1);
  eth_t flags = eth_arg2(args, eth_number_type);

  int fildes[2];
  if (pipe2(fildes, eth_num_val(flags)) < 0)
  {
    eth_throw(args, system_error(eth_str(eth_errno_to_str(errno))));
  }
  else
  {
    eth_t rx = eth_open_fd(fildes[0], "r");
    eth_t tx = eth_open_fd(fildes[1], "w");
    eth_return(args, eth_tup2(rx, tx));
  }
}

static eth_t
_fileno(void)
{
  eth_args args = eth_start(1);
  eth_t file = eth_arg2(args, eth_file_type);

  int fd = fileno(eth_get_file_stream(file));
  int err = errno;
  if (fd < 0)
    eth_throw(args, eth_system_error(err));
  else
    eth_return(args, eth_num(fd));
}


int
ether_module(eth_module *mod, eth_root *topenv)
{
  eth_module *detail = eth_create_module("os.detail", NULL);
  eth_copy_module_path(eth_get_root_env(topenv), eth_get_env(detail));

  eth_define(detail, "__chdir", eth_proc(_chdir, 1));

  eth_define(detail, "__f_ok", eth_num(F_OK));
  eth_define(detail, "__r_ok", eth_num(R_OK));
  eth_define(detail, "__w_ok", eth_num(W_OK));
  eth_define(detail, "__x_ok", eth_num(X_OK));
  eth_define(detail, "__access", eth_proc(_access, 2));
  eth_define(detail, "__getcwd", eth_proc(_getcwd));

  eth_define(detail, "__getenv", eth_proc(_getenv, 1));
  eth_define(detail, "__setenv", eth_proc(_setenv, 3));
  eth_define(detail, "__unsetenv", eth_proc(_unsetenv, 1));

  eth_define(detail, "__realpath", eth_proc(_realpath, 1));

  eth_define(detail, "__mkdtemp", eth_proc(_mkdtemp, 1));

  eth_define(detail, "__fork", eth_proc(_fork));
  eth_define(detail, "__waitpid", eth_proc(_waitpid, 2));

  eth_define(detail, "__pipe", eth_proc(_pipe));
  eth_define(detail, "__pipe2", eth_proc(_pipe2, 1));

  eth_define(detail, "__fileno", eth_proc(_fileno, 1));

  eth_define(detail, "__o_cloexec", eth_num(O_CLOEXEC));
  eth_define(detail, "__o_direct", eth_num(O_DIRECT));
  eth_define(detail, "__o_nonblock", eth_num(O_NONBLOCK));

  eth_module *aux = eth_load_module_from_script2(topenv, "./lib.eth", NULL, detail);
  eth_destroy_module(detail);
  if (not aux)
    return -1;

  eth_copy_defs(aux, mod);
  eth_destroy_module(aux);

  return 0;
}

