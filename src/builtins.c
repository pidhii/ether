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
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

ETH_MODULE("ether:builtins")


static eth_t
_strcat(void)
{
  eth_args args = eth_start(2);
  eth_t x = eth_arg2(args, eth_string_type);
  eth_t y = eth_arg2(args, eth_string_type);

  int xlen = eth_str_len(x);
  int ylen = eth_str_len(y);
  const char *xstr = eth_str_cstr(x);
  const char *ystr = eth_str_cstr(y);

  char *str = malloc(xlen + ylen + 1);
  memcpy(str, xstr, xlen);
  memcpy(str + xlen, ystr, ylen + 1);

  eth_end_unref(args);
  return eth_create_string_from_ptr2(str, xlen + ylen);
}

static eth_t
_and(void)
{
  eth_t x = *eth_sp++;
  eth_ref(x);
  eth_t y = *eth_sp++;
  eth_ref(y);
  if (x == eth_false)
  {
    eth_unref(y);
    eth_dec(x);
    return eth_false;
  }
  else
  {
    eth_dec(x);
    eth_dec(y);
    return y;
  }
}

static eth_t
_or(void)
{
  eth_t x = *eth_sp++;
  eth_ref(x);
  eth_t y = *eth_sp++;
  eth_ref(y);
  if (x == eth_false)
  {
    eth_dec(x);
    eth_dec(y);
    return y;
  }
  else
  {
    eth_unref(y);
    eth_dec(x);
    return x;
  }
}

static eth_t
_pair_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_pair_type);
  eth_drop(x);
  return ret;
}

static eth_t
_symbol_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_symbol_type);
  eth_drop(x);
  return ret;
}

static eth_t
_number_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_number_type);
  eth_drop(x);
  return ret;
}

static eth_t
_string_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_string_type);
  eth_drop(x);
  return ret;
}

static eth_t
_boolean_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_boolean_type);
  eth_drop(x);
  return ret;
}

static eth_t
_function_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_function_type);
  eth_drop(x);
  return ret;
}

static eth_t
_tuple_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(eth_is_tuple(x->type));
  eth_drop(x);
  return ret;
}

static eth_t
_record_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(eth_is_record(x->type));
  eth_drop(x);
  return ret;
}

static eth_t
_file_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_file_type);
  eth_drop(x);
  return ret;
}

static eth_t
_regexp_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_regexp_type);
  eth_drop(x);
  return ret;
}

static eth_t
_vector_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_vector_type);
  eth_drop(x);
  return ret;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  regexp
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_regexp_eq(void)
{
  /*eth_use_symbol(Regexp_error)*/
  /*eth_args args = eth_start(2);*/
  /*eth_t str = eth_arg2(args, eth_string_type);*/
  /*eth_t reg = eth_arg2(args, eth_regexp_type);*/
  /*int n = eth_exec_regexp(reg, eth_str_cstr(str), eth_str_len(str), 0);*/
  /*if (n == 0)*/
    /*eth_throw(args, Regexp_error);*/
  /*else*/
    /*eth_return(args, n < 0 ? eth_false : eth_num(n));*/

  eth_use_symbol(regexp_error)

  eth_args args = eth_start(2);
  eth_t str = eth_arg2(args, eth_string_type);
  eth_t reg = eth_arg2(args, eth_regexp_type);
  int n = eth_exec_regexp(reg, eth_str_cstr(str), eth_str_len(str), 0);
  if (n < 0)
    eth_return(args, eth_false);
  else if (n == 0)
    eth_throw(args, regexp_error);
  else if (n == 1)
    eth_return(args, eth_true);
  else
  {
    eth_t buf[n-1];
    const int *ovec = eth_ovector();
    for (int cnt = 1; cnt < n; ++cnt)
    {
      int i = cnt * 2;
      char *p = eth_str_cstr(str) + ovec[i];
      int len = ovec[i+1] - ovec[i];
      eth_t s = eth_create_string2(p, len);
      buf[cnt-1] = s;
    }

    if (n == 2)
      eth_return(args, buf[0]);
    else
    {
      eth_t ret = eth_create_tuple_n(eth_tuple_type(n-1), buf);
      eth_return(args, ret);
    }
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                   dupm
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_dump(void)
{
  eth_args args = eth_start(1);
  eth_t fn = eth_arg2(args, eth_function_type);
  char *ptr = NULL;
  size_t size = 0;
  FILE *out = open_memstream(&ptr, &size);
  eth_dump_ssa(ETH_FUNCTION(fn)->clos.src->ssa->body, out);
  fclose(out);
  eth_return(args, eth_create_string_from_ptr2(ptr, size));
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  lists
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_inclusive_range(void)
{
  eth_args args = eth_start(2);
  eth_t from = eth_arg2(args, eth_number_type);
  eth_t to = eth_arg2(args, eth_number_type);
  intmax_t f = eth_num_val(from);
  intmax_t t = eth_num_val(to);
  eth_t acc = eth_nil;
  for (intmax_t i = t; f <= i; --i)
    acc = eth_cons(eth_num(i), acc);
  eth_return(args, acc);
}

static eth_t
_map(void)
{
  eth_use_symbol(improper_list)

  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t l = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it;
  for (it = l; eth_is_pair(it); it = eth_cdr(it))
  {
    eth_reserve_stack(1);
    eth_sp[0] = eth_car(it);
    const eth_t v = eth_apply(f, 1);
    if (eth_unlikely(eth_is_exn(v)))
    {
      eth_drop(acc);
      eth_rethrow(args, v);
    }
    acc = eth_cons(v, acc);
  }
  if (eth_unlikely(it != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, improper_list);
  }

  eth_t ret = eth_reverse(acc);
  eth_drop(acc);
  eth_return(args, ret);
}

static eth_t
_filter_map(void)
{
  eth_use_symbol(filter_out)
  eth_use_symbol(improper_list)

  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t l = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it;
  for (it = l; eth_is_pair(it); it = eth_cdr(it))
  {
    eth_reserve_stack(1);
    eth_sp[0] = eth_car(it);
    const eth_t v = eth_apply(f, 1);
    if (eth_is_exn(v) && eth_what(v) == filter_out)
      eth_drop(v);
    else
      acc = eth_cons(v, acc);
  }
  if (eth_unlikely(it != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, improper_list);
  }

  eth_t ret = eth_reverse(acc);
  eth_drop(acc);
  eth_return(args, ret);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                   I/O
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_open(void)
{
  eth_use_variant(system_error);

  eth_args args = eth_start(2);
  eth_t path = eth_arg2(args, eth_string_type);
  eth_t mode = eth_arg2(args, eth_string_type);
  eth_t file = eth_open(eth_str_cstr(path), eth_str_cstr(mode));
  if (file == NULL)
  {
    eth_t exn;
    switch (errno)
    {
      case EINVAL: exn = eth_invalid_argument(); break;
      default: exn = eth_system_error(errno); break;
    }
    eth_throw(args, exn);
  }
  eth_return(args, file);
}

static eth_t
_popen(void)
{
  eth_args args = eth_start(2);
  eth_t path = eth_arg2(args, eth_string_type);
  eth_t mode = eth_arg2(args, eth_string_type);
  eth_t file = eth_open_pipe(eth_str_cstr(path), eth_str_cstr(mode));
  if (file == NULL or ferror(eth_get_file_stream(file)))
  {
    eth_t exn;
    switch (errno)
    {
      case EINVAL: exn = eth_invalid_argument(); break;
      default: exn = eth_system_error(errno); break;
    }
    eth_throw(args, exn);
  }
  eth_return(args, file);
}

static eth_t
_open_string(void)
{
  eth_args args = eth_start(1);
  eth_t str = eth_arg2(args, eth_string_type);
  FILE *stream = fmemopen(eth_str_cstr(str), eth_str_len(str), "r");
  eth_t file = eth_open_stream(stream);
  eth_set_file_data(file, str, (void*)eth_unref);
  eth_ref(str);
  eth_return(args, file);
}

static eth_t
_close(void)
{
  eth_t file = *eth_sp++;
  if (file->type != eth_file_type)
  {
    eth_drop(file);
    return eth_exn(eth_type_error());
  }
  if (eth_is_open(file))
  {
    if (eth_is_pipe(file))
    {
      errno = 0;
      int ret = eth_close(file);
      int err = errno;
      eth_drop(file);
      if (err)
        return eth_system_error(err);
      else
        return eth_num(ret);
    }
    else
    {
      errno = 0;
      int ret = eth_close(file);
      int err = errno;
      eth_drop(file);
      if (ret)
      {
        switch (err)
        {
          case EBADF: return eth_exn(eth_invalid_argument());
          default: return eth_system_error(err);
        }
      }
      else
        return eth_nil;
    }
  }
  else
  {
    eth_drop(file);
    return eth_nil;
  }
}

static eth_t
_input(void)
{
  eth_t prompt = *eth_sp++;

  bool istty = isatty(STDIN_FILENO);

  // Try detect EOF before printing promt:
  if (not istty)
  {
    char c = getc(stdin);
    if (c == EOF)
    {
      eth_drop(prompt);
      return eth_exn(eth_sym("End_of_file"));
    }
    ungetc(c, stdin);
  }

  eth_display(prompt, stdout);
  eth_drop(prompt);

  char *line = NULL;
  size_t n = 0;
  ssize_t nrd = getline(&line, &n, stdin);
  if (nrd < 0)
  {
    int err = errno;
    free(line);
    if (feof(stdin))
      return eth_exn(eth_sym("End_of_file"));
    else
    {
      switch (err)
      {
        case EINVAL: return eth_exn(eth_invalid_argument());
        default: return eth_system_error(err);
      }
    }
  }

  // Show entered line when input is fed from pipe:
  if (not istty)
    fputs(line, stdout);

  // Remove trailing newline-symbol:
  int len = nrd;
  if (line[len - 1] == '\n')
  {
    len = nrd - 1;
    line[len] = 0;
  }

  return eth_create_string_from_ptr2(line, len);
}

static eth_t
_print_to(void)
{
  eth_t file = *eth_sp++;
  eth_t x = *eth_sp++;

  if (eth_is_tuple(x->type))
  {
    for (int i = 0; i < eth_tuple_size(x->type); ++i)
    {
      if (i > 0) putc('\t', eth_get_file_stream(file));
      eth_display(eth_tup_get(x, i), eth_get_file_stream(file));
    }
  }
  else
    eth_display(x, eth_get_file_stream(file));
  putc('\n', eth_get_file_stream(file));
  eth_drop(file);
  eth_drop(x);
  return eth_nil;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  system
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_system(void)
{
  eth_args args = eth_start(1);
  eth_t cmd = eth_arg2(args, eth_string_type);
  errno = 0;
  int ret = system(eth_str_cstr(cmd));
  if (errno)
    eth_throw(args, eth_sym("System_error"));
  eth_return(args, eth_num(ret));
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  random
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_rand(void)
{ return eth_num(rand()); }

static eth_t
_srand(void)
{
  eth_t s = *eth_sp++;
  if (eth_unlikely(not eth_is_num(s)))
  {
    eth_drop(s);
    return eth_exn(eth_invalid_argument());
  }
  srand(eth_num_val(s));
  eth_drop(s);
  return eth_nil;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  format
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  eth_t file;
  eth_t fmt;
  int n;
} format_data;

static void
destroy_format_data(format_data *data)
{
  if (data->file)
    eth_unref(data->file);
  eth_unref(data->fmt);
  free(data);
}

static eth_t
_format_aux(void)
{
  eth_use_symbol(format_error);

  format_data *data = eth_this->proc.data;
  char *fmt = ETH_STRING(data->fmt)->cstr;
  int n = data->n;

  char *ptr = NULL;
  size_t size = 0;
  FILE *out = open_memstream(&ptr, &size);
  /*eth_t ret = _fmt(fmt, n, out);*/
  for (int i = 0; i < n; eth_ref(eth_sp[i++]));
  int ok = eth_format(out, fmt, eth_sp, n);
  for (int i = 0; i < n; eth_unref(eth_sp[i++]));
  eth_pop_stack(n);
  fclose(out);

  if (not ok)
  {
    free(ptr);
    return eth_exn(format_error);
  }
  else
    return eth_create_string_from_ptr2(ptr, size);
}

static eth_t
_format(void)
{
  eth_t fmt = *eth_sp++;
  if (fmt->type != eth_string_type)
  {
    eth_drop(fmt);
    return eth_exn(eth_type_error());
  }

  int n = eth_study_format(eth_str_cstr(fmt));
  if (n < 0)
  {
    eth_drop(fmt);
    return eth_exn(eth_sym("format_error"));
  }

  if (n == 0)
  {
    return fmt;
  }
  else
  {
    format_data *data = malloc(sizeof(format_data));
    eth_ref(data->fmt = fmt);
    data->file = NULL;
    data->n = n;
    return eth_create_proc(_format_aux, n, data, (void*)destroy_format_data);
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                exceptions
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_raise(void)
{
  return eth_exn(*eth_sp++);
}

static eth_t
__exit(void)
{
  eth_args args = eth_start(1);
  eth_t status = eth_arg2(args, eth_number_type);
  if (not eth_is_int(status))
    eth_throw(args, eth_invalid_argument());
  eth_t exitobj = eth_create_exit_object(eth_num_val(status));
  eth_throw(args, exitobj);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  load
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//*
static eth_t
_load(void)
{
  static int cnt = 1;
  eth_use_tuple_as(tup2, 2);

  eth_root *thisroot = eth_this->proc.data;

  char id[42];
  sprintf(id, "load.%d", cnt++);

  eth_t path = *eth_sp++;
  eth_t env = *eth_sp++;
  if (eth_unlikely(not eth_is_str(path)))
  {
    eth_drop_2(path, env);
    return eth_exn(eth_type_error());
  }

  eth_module *envmod = eth_create_module(NULL, NULL);
  for (eth_t it = env; eth_is_pair(it); it = eth_cdr(it))
  {
    eth_t def = eth_car(it);
    if (def->type != tup2)
    {
      eth_drop_2(path, env);
      eth_destroy_module(envmod);
      return eth_exn(eth_type_error());
    }
    eth_t key = eth_tup_get(def, 0);
    eth_t val = eth_tup_get(def, 1);
    if (not eth_is_str(key))
    {
      eth_drop_2(path, env);
      eth_destroy_module(envmod);
      return eth_exn(eth_type_error());
    }
    eth_define(envmod, eth_str_cstr(key), val);
  }

  FILE *fs = fopen(eth_str_cstr(path), "r");
  if (fs == NULL)
  {
    eth_drop_2(path, env);
    eth_destroy_module(envmod);
    return eth_exn(eth_system_error(ENOENT));
  }

  eth_ast *ast = eth_parse(thisroot, fs);
  fclose(fs);
  if (not ast)
  {
    eth_drop_2(path, env);
    eth_destroy_module(envmod);
    return eth_exn(eth_failure());
  }

  eth_module *mod = eth_create_module(id, NULL);
  eth_t ret;
  int ok = eth_load_module_from_ast2(thisroot, mod, ast, &ret, envmod);
  eth_destroy_module(envmod);
  eth_drop_ast(ast);

  if (not ok)
  {
    eth_drop_2(path, env);
    eth_destroy_module(mod);
    return eth_exn(eth_failure());
  }

  eth_t acc = eth_nil;
  int ndefs = eth_get_ndefs(mod);
  eth_def defs[ndefs];
  eth_get_defs(mod, defs);
  for (int i = 0; i < ndefs; ++i)
    acc = eth_cons(eth_tup2(eth_str(defs[i].ident), defs[i].val), acc);

  ret = eth_tup2(ret, acc);
  eth_drop_2(path, env);
  eth_destroy_module(mod);
  return ret;
}
// */

//*
static eth_t
_load_stream(void)
{
  static int cnt = 1;
  eth_use_tuple_as(tup2, 2);

  eth_root *thisroot = eth_this->proc.data;

  eth_t file = *eth_sp++;
  eth_t env = *eth_sp++;
  if (eth_unlikely(not eth_is_file(file)))
    goto error_1;

  eth_module *envmod = eth_create_module(NULL, NULL);
  for (eth_t it = env; eth_is_pair(it); it = eth_cdr(it))
  {
    eth_t def = eth_car(it);
    if (def->type != tup2)
      goto error_2;

    eth_t key = eth_tup_get(def, 0);
    eth_t val = eth_tup_get(def, 1);
    if (not eth_is_str(key))
      goto error_2;

    eth_define(envmod, eth_str_cstr(key), val);
  }

  char id[42];
  sprintf(id, "load.%d", cnt++);

  eth_ast *ast = eth_parse(thisroot, eth_get_file_stream(file));
  if (not ast)
    goto error_3;

  eth_module *mod = eth_create_module(id, NULL);
  eth_t ret;
  int ok = eth_load_module_from_ast2(thisroot, mod, ast, &ret, envmod);
  eth_destroy_module(envmod);
  eth_drop_ast(ast);
  if (not ok)
    goto error_4;

  {
    eth_t acc = eth_nil;
    int ndefs = eth_get_ndefs(mod);
    eth_def defs[ndefs];
    eth_get_defs(mod, defs);
    for (int i = 0; i < ndefs; ++i)
      acc = eth_cons(eth_tup2(eth_str(defs[i].ident), defs[i].val), acc);

    ret = eth_tup2(ret, acc);
    eth_drop_2(file, env);
    eth_destroy_module(mod);
    return ret;
  }

error_1:
  eth_drop_2(file, env);
  return eth_exn(eth_type_error());

error_2:
  eth_drop_2(file, env);
  eth_destroy_module(envmod);
  return eth_exn(eth_type_error());

error_3:
  eth_drop_2(file, env);
  eth_destroy_module(envmod);
  return eth_exn(eth_failure());

error_4:
  eth_drop_2(file, env);
  eth_destroy_module(mod);
  return eth_exn(eth_failure());
}
// */

static eth_t
_require(void)
{
  eth_root *thisroot = eth_this->proc.data;

  eth_t modpath = *eth_sp++;
  if (eth_unlikely(not eth_is_str(modpath)))
  {
    eth_drop(modpath);
    return eth_exn(eth_type_error());
  }

  eth_module *mod = eth_require_module(thisroot, eth_get_root_env(thisroot),
      eth_str_cstr(modpath));

  if (not mod)
  {
    eth_drop(modpath);
    return eth_exn(eth_failure());
  }

  int ndefs = eth_get_ndefs(mod);
  eth_def defs[ndefs];
  eth_get_defs(mod, defs);
  char *keys[ndefs];
  eth_t vals[ndefs];
  for (int i = 0; i < ndefs; ++i)
  {
    keys[i] = defs[i].ident;
    vals[i] = defs[i].val;
  }
  eth_t ret = eth_record(keys, vals, ndefs);

  eth_drop(modpath);
  return ret;
}
// */

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  lazy
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_lazy_get(void)
{
  return eth_this->proc.data;
}

static eth_t
_lazy_eval(void)
{
  eth_function *this = eth_this;
  eth_t thunk = this->proc.data;
  eth_t val = eth_apply(thunk, 0);
  if (eth_unlikely(eth_is_exn(val)))
    return val;
  // replace handle
  this->proc.handle = _lazy_get;
  this->proc.data = val;
  this->proc.dtor = (void*)eth_unref;
  eth_ref(val);
  eth_unref(thunk);
  return val;
}

static eth_t
_make_lazy(void)
{
  eth_t thunk = *eth_sp++;
  if (eth_unlikely(not eth_is_fn(thunk)))
  {
    eth_drop(thunk);
    return eth_exn(eth_type_error());
  }
  eth_ref(thunk);
  return eth_create_proc(_lazy_eval, 0, thunk, (void*)eth_unref);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                 sequences
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

static eth_t
_list(void)
{
  eth_t x = *eth_sp++;
  if (x == eth_nil || x->type == eth_pair_type)
  {
    return x;
  }
  else if (x->type == eth_string_type)
  {
    eth_t acc = eth_nil;
    const char *str = eth_str_cstr(x);
    for (int i = eth_str_len(x) - 1; i >= 0; --i)
      acc = eth_cons(eth_create_string_from_char(str[i]), acc);
    eth_drop(x);
    return acc;
  }
  else if (eth_is_tuple(x->type))
  {
    int n = eth_tuple_size(x->type);
    eth_t acc = eth_nil;
    for (int i = n - 1; i >= 0; --i)
      acc = eth_cons(eth_tup_get(x, i), acc);
    eth_drop(x);
    return acc;
  }
  else if (eth_is_record(x->type))
  {
    int n = eth_record_size(x->type);
    eth_t acc = eth_nil;
    for (int i = n - 1; i >= 0; --i)
    {
      eth_t key = eth_str(x->type->fields[i].name);
      eth_t val = eth_tup_get(x, i);
      acc = eth_cons(eth_tup2(key, val), acc);
    }
    eth_drop(x);
    return acc;
  }
  else if (eth_is_vec(x))
  {
    // TODO: optimize
    int n = eth_vec_len(x);
    eth_t acc = eth_nil;
    for (int i = n - 1; i >= 0; --i)
      acc = eth_cons(eth_vec_get(x, i), acc);
    eth_drop(x);
    return acc;
  }
  else
  {
    eth_drop(x);
    return eth_exn(eth_failure());
  }
}

static eth_t
_len(void)
{
  eth_t s = *eth_sp++;

  if (s == eth_nil)
  {
    return eth_num(0);
  }
  else if (eth_is_pair(s))
  {
    bool isproper;
    size_t len = eth_length(s, &isproper);
    eth_drop(s);
    if (isproper)
      return eth_num(len);
    else
      return eth_exn(eth_invalid_argument());
  }
  else if (eth_is_str(s))
  {
    eth_t ret = eth_num(eth_str_len(s));
    eth_drop(s);
    return ret;
  }
  else if (eth_is_vec(s))
  {
    int len = eth_vec_len(s);
    eth_drop(s);
    return eth_num(len);
  }
  else
  {
    eth_drop(s);
    return eth_exn(eth_type_error());
  }
}

static eth_t
unpack_index(eth_t idx, int seqlen, int *k)
{
  eth_use_symbol(range_error)

  if (eth_unlikely(not eth_is_int(idx)))
    return eth_exn(eth_type_error());

  *k = eth_num_val(idx);
  if (*k < 0)
    *k = seqlen - *k;

  if (eth_unlikely(*k >= seqlen))
    return eth_exn(range_error);

  return NULL;
}

static eth_t
unpack_range(eth_t idx, int seqlen, int *kl, int *kr)
{
  eth_use_symbol(range_error);
  if (eth_is_rangelr(idx))
  {
    eth_t l = ETH_RANGELR(idx)->l;
    eth_t r = ETH_RANGELR(idx)->r;
    if (eth_unlikely(not eth_is_int(l) or not eth_is_int(r)))
      return eth_exn(eth_type_error());
    *kl = eth_num_val(l);
    *kr = eth_num_val(r);
  }
  else if (eth_is_rangel(idx))
  {
    eth_t l = ETH_RANGEL(idx)->l;
    if (eth_unlikely(not eth_is_int(l)))
      return eth_exn(eth_type_error());
    *kl = eth_num_val(l);
    *kr = seqlen - 1;
  }
  else if (eth_is_ranger(idx))
  {
    eth_t r = ETH_RANGER(idx)->r;
    if (eth_unlikely(not eth_is_int(r)))
      return eth_exn(eth_type_error());
    *kl = 0;
    *kr = eth_num_val(r);
  }

  if (*kl < 0)
    *kl = seqlen + *kl;
  if (*kr < 0)
    *kr = seqlen + *kr;

  if (eth_unlikely(*kl >= seqlen or *kr >= seqlen))
    return eth_exn(range_error);

  return NULL;
}

static eth_t
_index_vector(eth_t vec, eth_t idx)
{
  eth_t exn;
  if (eth_likely(eth_is_num(idx)))
  {
    int k;
    if ((exn = unpack_index(idx, eth_vec_len(vec), &k)))
      return exn;
    return eth_vec_get(vec, k);
  }
  else if (eth_is_range(idx))
  {
    int kl, kr;
    if ((exn = unpack_range(idx, eth_vec_len(vec), &kl, &kr)))
      return exn;

    eth_t ret = eth_create_vector();
    if (kl <= kr)
    {
      for (int i = kl; i <= kr; ++i)
        eth_push_mut(ret, eth_vec_get(vec, i));
    }
    else
    {
      for (int i = kl; i >= kr; --i)
        eth_push_mut(ret, eth_vec_get(vec, i));
    }
    return ret;
  }
  else
    return eth_exn(eth_type_error());
}

static eth_t
_index_string(eth_t str, eth_t idx)
{
  eth_t exn;
  if (eth_is_num(idx))
  {
    int k;
    if ((exn = unpack_index(idx, eth_str_len(str), &k)))
      return exn;
    return eth_create_string2(eth_str_cstr(str) + k, 1);
  }
  else if (eth_is_range(idx))
  {
    int kl, kr;
    if ((exn = unpack_range(idx, eth_str_len(str), &kl, &kr)))
      return exn;

    if (kl <= kr)
      return eth_create_string2(eth_str_cstr(str) + kl, kr - kl + 1);
    else
    {
      int n = kl - kr + 1;
      char *buf = malloc(n + 1);
      for (int i = 0; i < n; ++i)
        buf[n - i - 1] = eth_str_cstr(str)[kr + i];
      buf[n] = '\0';
      return eth_create_string_from_ptr2(buf, n);
    }
  }
  else
    return eth_exn(eth_type_error());
}

static eth_t
_index_list(eth_t l, eth_t idx)
{
  eth_t exn;
  if (eth_is_num(idx))
  {
    int k;
    if ((exn = unpack_index(idx, eth_length(l, NULL), &k)))
      return exn;
    while (k--)
      l = eth_cdr(l);
    return eth_car(l);
  }
  else if (eth_is_range(idx))
  {
    int kl, kr;
    if ((exn = unpack_range(idx, eth_length(l, NULL), &kl, &kr)))
      return exn;

    if (kl <= kr)
    {
      int newlen = kr - kl + 1;
      while (--kl)
        l = eth_cdr(l);
      eth_t buf[newlen];
      for (int i = 0; i < newlen; ++i)
      {
        buf[i] = eth_car(l);
        l = eth_cdr(l);
      }
      eth_t acc = eth_nil;
      for (int i = newlen - 1; i >= 0; --i)
        acc = eth_cons(buf[i], acc);
      return acc;
    }
    else
    {
      int newlen = kr - kl + 1;
      while (--kl)
        l = eth_cdr(l);
      eth_t acc = eth_nil;
      for (int i = 0; i < newlen; ++i)
      {
        acc = eth_cons(eth_car(l), acc);
        l = eth_cdr(l);
      }
      return acc;
    }
  }
  else
    return eth_exn(eth_type_error());
}

static eth_t
_get(void)
{
  eth_t seq = *eth_sp++;
  eth_t idx = *eth_sp++;

  eth_t ret;
  if (eth_is_vec(seq))
    ret = _index_vector(seq, idx);
  else if (eth_is_str(seq))
    ret = _index_string(seq, idx);
  else if (eth_is_pair(seq))
    ret = _index_list(seq, idx);
  else
    ret = eth_exn(eth_type_error());

  eth_ref(ret);
  eth_drop_2(seq, idx);
  eth_dec(ret);
  return ret;
}

static eth_t
_rev_list(void)
{
  eth_t l = *eth_sp++;
  eth_t ret = eth_reverse(l);
  eth_drop(l);
  return ret;
}

static eth_t
_create_ref(void)
{
  eth_t x = *eth_sp++;
  eth_t ref = eth_create_strong_ref(x);
  return ref;
}

static eth_t
_dereference(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_strong_ref_type))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  eth_t ret = eth_ref_get(x);
  eth_ref(ret);
  eth_drop(x);
  eth_dec(ret);
  return ret;
}

static eth_t
_assign(void)
{
  eth_args args = eth_start(2);
  eth_t ref = eth_arg(args);
  eth_t x = eth_arg(args);
  if (ref->type == eth_strong_ref_type)
    eth_set_strong_ref(ref, x);
  else
    eth_throw(args, eth_exn(eth_type_error()));
  eth_return(args, eth_nil);
}

static eth_t
_shell(void)
{
  eth_args args = eth_start(1);
  eth_t cmd = eth_arg2(args, eth_string_type);
  FILE *input = popen(eth_str_cstr(cmd), "r");
  int totsize = 0;
  cod_vec(void*) parts;
  cod_vec_init(parts);
  while (true)
  {
    void *buf = malloc(256 + sizeof(int));
    char *str = buf;

    int nrd = fread(str, 1, 256, input);
    if (nrd < 256 && ferror(input))
    {
      free(buf);
      goto error;
    }
    *(int*)(buf+256) = nrd;
    totsize += nrd;
    cod_vec_push(parts, buf);
  }

  if (parts.len == 1)
  {
    void *buf = parts.data[0];
    int size = *(int*)(buf+256);
    eth_t ret = eth_create_string_from_ptr2(buf+sizeof(int), size);
  }

error:
  pclose(input);
  cod_vec_iter(parts, i, x, free(x));
  cod_vec_destroy(parts);
  eth_return(args, eth_system_error(0));
}

static eth_t
_record(void)
{
  eth_use_tuple_as(tup2, 2);

  eth_t list = *eth_sp++;

  bool isproper;
  int n = eth_length(list, &isproper);
  if (not isproper)
  {
    eth_drop(list);
    return eth_exn(eth_invalid_argument());
  }

  // extract key/value pairs
  char *keys[n];
  eth_t vals[n];
  int i = 0;
  for (eth_t it = list; it != eth_nil; it = eth_cdr(it), ++i)
  {
    eth_t x = eth_car(it);
    if (eth_unlikely(x->type != tup2))
    {
      eth_drop(list);
      return eth_exn(eth_invalid_argument());
    }

    eth_t key = eth_tup_get(x, 0);
    eth_t val = eth_tup_get(x, 1);
    if (eth_unlikely(not eth_is_str(key)))
    {
      eth_drop(list);
      return eth_exn(eth_invalid_argument());
    }

    keys[i] = (char*)eth_sym_cstr(key);
    vals[i] = val;
  }

  // remove duplicates (given two equal keys use the latest one)

  eth_t ret = eth_record(keys, vals, n);
  eth_drop(list);
  return ret;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                 module
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/*
 * NOTE: IR-builder will use builtins ...while loading builtins.
 */
eth_module*
eth_create_builtins(eth_root *root)
{
  char buf[PATH_MAX];

  eth_module *mod = eth_create_module("builtins", NULL);
  if (eth_get_module_path())
    eth_add_module_path(eth_get_env(mod), eth_get_module_path());

  extern void _eth_set_builtins(eth_root*, eth_module*);
  _eth_set_builtins(root, mod);

  eth_debug("loading builtins");

  eth_attr *attr = eth_create_attr(ETH_ATTR_BUILTIN);

  eth_define_attr(mod,    "&&", eth_create_proc(_and, 2, NULL, NULL), attr);
  eth_define_attr(mod,    "||", eth_create_proc( _or, 2, NULL, NULL), attr);
  // TODO: `++` should be an instruction
  eth_define(mod,         "++", eth_create_proc(    _strcat, 2, NULL, NULL));
  // ---
  eth_define(mod,      "pair?", eth_create_proc(    _pair_p, 1, NULL, NULL));
  eth_define(mod,    "symbol?", eth_create_proc(  _symbol_p, 1, NULL, NULL));
  eth_define(mod,    "number?", eth_create_proc(  _number_p, 1, NULL, NULL));
  eth_define(mod,    "string?", eth_create_proc(  _string_p, 1, NULL, NULL));
  eth_define(mod,   "boolean?", eth_create_proc( _boolean_p, 1, NULL, NULL));
  eth_define(mod,  "function?", eth_create_proc(_function_p, 1, NULL, NULL));
  eth_define(mod,     "tuple?", eth_create_proc(   _tuple_p, 1, NULL, NULL));
  eth_define(mod,    "record?", eth_create_proc(  _record_p, 1, NULL, NULL));
  eth_define(mod,      "file?", eth_create_proc(    _file_p, 1, NULL, NULL));
  eth_define(mod,    "regexp?", eth_create_proc(  _regexp_p, 1, NULL, NULL));
  eth_define(mod,    "vector?", eth_create_proc(  _vector_p, 1, NULL, NULL));
  // ---
  eth_define(mod,       "list", eth_create_proc(      _list, 1, NULL, NULL));
  eth_define(mod, "__rev_list", eth_create_proc(  _rev_list, 1, NULL, NULL));
  eth_define(mod,   "__record", eth_create_proc(    _record, 1, NULL, NULL));
  // ---
  eth_define(mod,       "dump", eth_create_proc(      _dump, 1, NULL, NULL));
  // ---
  eth_define(mod,         "=~", eth_create_proc( _regexp_eq, 2, NULL, NULL));
  // ---
  eth_define(mod, "__inclusive_range", eth_create_proc(_inclusive_range, 2, NULL, NULL));
  eth_define(mod, "__List_map", eth_create_proc(_map, 2, NULL, NULL));
  eth_define(mod, "__List_filter_map", eth_create_proc(_filter_map, 2, NULL, NULL));
  // ---
  eth_define(mod,      "stdin", eth_stdin);
  eth_define(mod,     "stdout", eth_stdout);
  eth_define(mod,     "stderr", eth_stderr);
  eth_define(mod,     "__open", eth_create_proc(      _open, 2, NULL, NULL));
  eth_define(mod,    "__popen", eth_create_proc(     _popen, 2, NULL, NULL));
  eth_define(mod, "__open_string", eth_create_proc(_open_string, 1, NULL, NULL));
  eth_define(mod,      "close", eth_create_proc(     _close, 1, NULL, NULL));
  eth_define(mod,      "input", eth_create_proc(     _input, 1, NULL, NULL));
  eth_define(mod, "__print_to", eth_create_proc(  _print_to, 2, NULL, NULL));
  // ---
  eth_define(mod,   "__system", eth_create_proc(    _system, 1, NULL, NULL));
  // ---
  eth_define(mod,     "__rand", eth_create_proc(      _rand, 0, NULL, NULL));
  eth_define(mod,    "__srand", eth_create_proc(     _srand, 1, NULL, NULL));
  // ---
  eth_define(mod,     "format", eth_create_proc(    _format, 1, NULL, NULL));
  // ---
  eth_define(mod,      "raise", eth_create_proc(     _raise, 1, NULL, NULL));
  eth_define(mod,       "exit", eth_create_proc(     __exit, 1, NULL, NULL));
  // ---
  eth_define(mod, "__load", eth_create_proc(_load, 2, root, NULL));
  eth_define(mod, "__load_stream", eth_create_proc(_load_stream, 2, root, NULL));
  eth_define(mod, "__require", eth_create_proc(_require, 1, root, NULL));
  // ---
  eth_define(mod, "__make_lazy", eth_create_proc(_make_lazy, 1, NULL, NULL));
  // ---
  eth_define(mod, "__create_ref", eth_create_proc(_create_ref, 1, NULL, NULL));
  eth_define(mod, "__dereference", eth_create_proc(_dereference, 1, NULL, NULL));
  eth_define(mod, "__assign", eth_create_proc(_assign, 2, NULL, NULL));


  if (not eth_resolve_path(eth_get_env(mod), "__builtins.eth", buf))
  {
    eth_warning("can't find builtins");
    eth_destroy_module(mod);
    return NULL;
  }

  eth_debug("- loading \"%s\"", buf);
  eth_module *auxmod = eth_load_module_from_script(root, buf, NULL);
  if (not auxmod)
  {
    eth_warning("failed to load builtins");
    eth_destroy_module(mod);
    return NULL;
  }
  eth_copy_defs(auxmod, mod);
  eth_destroy_module(auxmod);

  return mod;
}

