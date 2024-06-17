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
#include "eco/eco.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#include <sys/wait.h>

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

  char *str = eth_malloc(xlen + ylen + 1);
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
_dict_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_rbtree_type);
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
  if (not ETH_FUNCTION(fn)->islam)
    eth_return(args, eth_str("error: not a lambda"));
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
  for (it = l; it->type == eth_pair_type; it = eth_cdr(it))
  {
    eth_reserve_stack(1);
    eth_sp[0] = eth_car(it);
    const eth_t v = eth_apply(f, 1);
    if (eth_unlikely(v->type == eth_exception_type))
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
  for (it = l; it->type == eth_pair_type; it = eth_cdr(it))
  {
    eth_reserve_stack(1);
    eth_sp[0] = eth_car(it);
    const eth_t v = eth_apply(f, 1);
    if (v->type == eth_exception_type && eth_what(v) == filter_out)
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

  /*if (eth_is_tuple(x->type))*/
  /*{*/
    /*for (int i = 0; i < eth_struct_size(x->type); ++i)*/
    /*{*/
      /*if (i > 0) putc('\t', eth_get_file_stream(file));*/
      /*eth_display(eth_tup_get(x, i), eth_get_file_stream(file));*/
    /*}*/
  /*}*/
  /*else*/
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
    eth_throw(args, eth_sym("system_error"));
  eth_return(args, eth_num(WEXITSTATUS(ret)));
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
  if (eth_unlikely(s->type != eth_number_type))
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
    format_data *data = eth_malloc(sizeof(format_data));
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
  if (eth_unlikely(path->type != eth_string_type))
  {
    eth_drop_2(path, env);
    return eth_exn(eth_type_error());
  }

  eth_module *envmod = eth_create_module(NULL, NULL);
  for (eth_t it = env; it->type == eth_pair_type; it = eth_cdr(it))
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
    if (key->type != eth_string_type)
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
  if (eth_unlikely(file->type != eth_file_type))
    goto error_1;

  eth_module *envmod = eth_create_module(NULL, NULL);
  for (eth_t it = env; it->type == eth_pair_type; it = eth_cdr(it))
  {
    eth_t def = eth_car(it);
    if (def->type != tup2)
      goto error_2;

    eth_t key = eth_tup_get(def, 0);
    eth_t val = eth_tup_get(def, 1);
    if (key->type != eth_string_type)
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

  eth_args args = eth_start(1);
  eth_t modpath = eth_arg2(args, eth_string_type);

  eth_module *mod = eth_require_module(thisroot, eth_get_root_env(thisroot),
      eth_str_cstr(modpath));

  if (not mod)
    eth_throw(args, eth_failure());

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

  eth_return(args, ret);
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
  if (eth_unlikely(val->type == eth_exception_type))
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
  if (eth_unlikely(thunk->type != eth_function_type))
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
  eth_t ret = eth_list(x);
  if (ret)
  {
    eth_ref(ret);
    eth_drop(x);
    eth_dec(ret);
    return ret;
  }
  else
  {
    eth_drop(x);
    return eth_exn(eth_invalid_argument());
  }
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
    void *buf = eth_malloc(256 + sizeof(int));
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
    if (eth_unlikely(key->type != eth_string_type))
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

#define COROUTINES 1
#if COROUTINES

static eth_type *_state_obj_type;


typedef struct _state_obj {
  eth_header hdr;
  eth_state *state;
  eth_t entry;
  bool full_destroy_state;
} _state_obj;
#define SOBJ(x) ((_state_obj*)(x))
#define GET_STATE(x) ((_state_obj*)(x))->state

static eth_t
_make_state_obj(eth_state *state, eth_t entry, bool full_destroy_state)
{
  _state_obj *sobj = eth_malloc(sizeof(_state_obj));
  memset(sobj, 0, sizeof(_state_obj));
  eth_init_header(ETH(sobj), _state_obj_type);
  sobj->state = state;
  sobj->state->data = sobj;
  if (entry)
    eth_ref(sobj->entry = entry);
  sobj->full_destroy_state = full_destroy_state;
  return ETH(sobj);
}

static void
_destroy_state_obj(eth_type *type, eth_t x)
{
  _state_obj *sobj = (_state_obj*)x;

  if (sobj->full_destroy_state)
    eth_destroy_state(sobj->state);
  else
    free(sobj->state);

  if (sobj->entry)
    eth_unref(sobj->entry);

  free(sobj);
}


static eth_t
_g_current_state_box;

static eth_t
_get_current_state(void)
{
  return eth_ref_get(_g_current_state_box);
}

static eth_t
_switch_state(void)
{
  eth_args args = eth_start(3);
  eth_t from = eth_arg2(args, _state_obj_type);
  eth_t to = eth_arg2(args, _state_obj_type);
  eth_t parcel = eth_arg(args); // TODO: remove ref

  if (eth_unlikely(from == to))
    eth_throw(args, eth_invalid_argument());

  eth_state *ret_caller;
  eth_t ret_parcel;
  eth_set_strong_ref(_g_current_state_box, to);
  if (not eth_switch_state(GET_STATE(from), GET_STATE(to), parcel,
        &ret_caller, (void**)&ret_parcel))
  {
    eth_set_strong_ref(_g_current_state_box, from);
    eth_return(args, eth_false);
  }
  else
  {
    eth_t ret = eth_tup2(ret_caller->data, ret_parcel);
    eth_return(args, ret);
  }
}

static void
_co_entry_point(eth_state *this_state, eth_state *caller_state, eth_t parcel)
{
  eth_t this_sobj = this_state->data;
  eth_t caller_sobj = caller_state->data;

  eth_reserve_stack(3);
  eth_sp[0] = this_sobj;
  eth_sp[1] = caller_sobj;
  eth_sp[2] = parcel;
  eth_t ret = eth_apply(SOBJ(this_sobj)->entry, 3);
  if (ret->type == eth_exception_type)
    eth_warning("<coroutine> returned an unhandled exception: ~w", ret);
  eth_drop(ret);
}

static eth_t
_make_initial_state(void)
{
  eth_args args = eth_start(1);
  eth_t entry = eth_arg2(args, eth_function_type);

  eth_state *state = eth_create_initial_state(2, _co_entry_point);
  eth_t sobj = _make_state_obj(state, entry, true);

  eth_return(args, sobj);
}
#endif

static eth_t
_make_method(void)
{
  eth_args args = eth_start(2);
  eth_t arity = eth_arg2(args, eth_number_type);
  eth_t default_impl = eth_arg(args);
  eth_return(args, eth_create_method(
        eth_num_val(arity), default_impl == eth_nil ? NULL : default_impl));
                                      
}

static eth_t
_implements(void)
{
  eth_args args = eth_start(2);
  eth_t x = eth_arg(args);
  eth_t method = eth_arg2(args, eth_method_type);
  eth_return(args, eth_boolean(eth_find_method(x->type->methods, method)));
}

static eth_t
_make_struct(void)
{
  eth_args args = eth_start(2);
  eth_t base = eth_arg(args);
  eth_t methods = eth_arg(args);

  int nfields = eth_length(base, NULL);
  char *keys[nfields];
  eth_t vals[nfields];
  for (int i = 0; base->type == eth_pair_type; base = eth_cdr(base), ++i)
  {
    keys[i] = eth_str_cstr(eth_car(base));
    vals[i] = eth_nil;
  }
  eth_type *type = eth_unique_record_type(keys, nfields);

  for (; methods->type == eth_pair_type; methods = eth_cdr(methods))
  {
    eth_t keyval = eth_car(methods);
    eth_t method = eth_tup_get(keyval, 0);
    eth_t impl = eth_tup_get(keyval, 1);
    if (method->type != eth_method_type)
    {
      eth_destroy_type(type);
      eth_throw(args, eth_type_error());
    }

    if (impl == eth_nil)
      impl = eth_get_method_default(method);
    else if (ETH_FUNCTION(impl)->arity < eth_get_method_arity(method))
    {
      eth_destroy_type(type);
      eth_throw(args, eth_type_error());
    }

    eth_add_method(type->methods, method, impl);
  }

  eth_t ret = eth_create_record(type, vals);
  eth_return(args, ret);
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


#if COROUTINES
  _state_obj_type = eth_create_type("coroutine");
  _state_obj_type->destroy = _destroy_state_obj;

  eth_state *initial_state = eth_malloc(sizeof(eth_state));
  memset(initial_state, 0, sizeof(eth_state));
  eth_t initial_sobj = _make_state_obj(initial_state, NULL, false);

  _g_current_state_box = eth_create_strong_ref(initial_sobj);
  eth_ref(_g_current_state_box);

  eth_add_destructor(mod, _g_current_state_box, (void*)eth_unref);
  eth_add_destructor(mod, _state_obj_type, (void*)eth_destroy_type);
#endif


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
  eth_define(mod,      "dict?", eth_create_proc(    _dict_p, 1, NULL, NULL));
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

  eth_define(mod, "__make_method", eth_create_proc(_make_method, 2, NULL, NULL));
  eth_define(mod, "__make_struct", eth_create_proc(_make_struct, 2, NULL, NULL));
  eth_define(mod, "implements", eth_create_proc(_implements, 2, NULL, NULL));
  eth_define(mod, "apply", eth_apply_method);
  eth_define(mod, "access", eth_get_method);
  eth_define(mod, "write", eth_write_method);
  eth_define(mod, "display", eth_display_method);
  eth_define(mod, "len", eth_len_method);
  eth_define(mod, "cmp", eth_cmp_method);

  eth_define(mod, "dict", eth_create_rbtree());

#if COROUTINES
  eth_define(mod, "__switch_state", eth_create_proc(_switch_state, 3, NULL, NULL));
  eth_define(mod, "__make_initial_state", eth_create_proc(_make_initial_state, 1, NULL, NULL));
  eth_define(mod, "__get_current_state", eth_create_proc(_get_current_state, 0, NULL, NULL));
#endif


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

