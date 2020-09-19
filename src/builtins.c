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


static
eth_env *g_env;

static
eth_module *g_mod;


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
  else
  {
    eth_drop(x);
    return eth_exn(eth_failure());
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  regexp
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_regexp_eq(void)
{
  eth_use_symbol(Regexp_error)
  eth_args args = eth_start(2);
  eth_t str = eth_arg2(args, eth_string_type);
  eth_t reg = eth_arg2(args, eth_regexp_type);
  int n = eth_exec_regexp(reg, eth_str_cstr(str), eth_str_len(str), 0);
  if (n == 0)
    eth_throw(args, Regexp_error);
  else
    eth_return(args, n < 0 ? eth_false : eth_num(n));
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
  eth_use_symbol(Improper_list)

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
    eth_throw(args, Improper_list);
  }

  eth_t ret = eth_reverse(acc);
  eth_drop(acc);
  eth_return(args, ret);
}

static eth_t
_filter_map(void)
{
  eth_use_symbol(Filter_out)
  eth_use_symbol(Improper_list)

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
    if (eth_is_exn(v) && eth_what(v) == Filter_out)
      eth_drop(v);
    else
      acc = eth_cons(v, acc);
  }
  if (eth_unlikely(it != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, Improper_list);
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
  eth_use_variant(System_error);

  eth_args args = eth_start(2);
  eth_t path = eth_arg2(args, eth_string_type);
  eth_t mode = eth_arg2(args, eth_string_type);
  eth_t file = eth_open(eth_str_cstr(path), eth_str_cstr(mode));
  if (file == NULL)
  {
    eth_t exn;
    switch (errno)
    {
      case EINVAL: exn = eth_exn(eth_invalid_argument()); break;
      default: exn = eth_system_error(errno); break;
    }
    eth_return(args, exn);
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
  if (file == NULL)
  {
    eth_t exn;
    switch (errno)
    {
      case EINVAL: exn = eth_exn(eth_invalid_argument()); break;
      default: exn = eth_system_error(errno); break;
    }
    eth_return(args, exn);
  }
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
_write_to(void)
{
  eth_args args = eth_start(2);
  eth_t file = eth_arg2(args, eth_file_type);
  eth_t str = eth_arg2(args, eth_string_type);
  FILE *stream = eth_get_file_stream(file);
  clearerr(stream);
  errno = 0;
  size_t nwr = fwrite(eth_str_cstr(str), 1, eth_str_len(str), stream);
  if (ferror(stream))
  {
    switch (errno)
    {
      case EINVAL:
      case EBADF:
        eth_throw(args, eth_invalid_argument());
    }
    eth_throw(args, eth_sym("System_error"));
  }
  eth_end_unref(args);
  return eth_nil;
}

static eth_t
_newline(void)
{
  putchar('\n');
  return eth_nil;
}

static eth_t
_print(void)
{
  eth_t x = *eth_sp++;

  if (eth_is_tuple(x->type))
  {
    for (int i = 0; i < eth_tuple_size(x->type); ++i)
    {
      if (i > 0) putc('\t', stdout);
      eth_display(eth_tup_get(x, i), stdout);
    }
  }
  else
    eth_display(x, stdout);
  putc('\n', stdout);
  eth_drop(x);
  return eth_nil;
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
_read_line_of(void)
{
  eth_t file = *eth_sp++;
  if (file->type != eth_file_type)
  {
    eth_drop(file);
    return eth_exn(eth_type_error());
  }

  char *line = NULL;
  size_t n = 0;
  ssize_t nrd = getline(&line, &n, eth_get_file_stream(file));
  int err = errno;

  if (nrd < 0)
  {
    free(line);
    if (feof(eth_get_file_stream(file)))
    {
      eth_drop(file);
      return eth_exn(eth_sym("End_of_file"));
    }
    else
    {
      eth_drop(file);
      switch (err)
      {
        case EBADF:
        case EINVAL: return eth_exn(eth_invalid_argument());
        default: return eth_system_error(err);
      }
    }
  }
  return eth_create_string_from_ptr2(line, nrd);
}

static eth_t
_read_of(void)
{
  eth_t file = *eth_sp++;
  eth_ref(file);

  eth_t n = *eth_sp++;
  eth_ref(n);

  if (file->type != eth_file_type || not eth_is_num(n))
  {
    eth_unref(file);
    eth_unref(n);
    return eth_exn(eth_type_error());
  }
  if (eth_num_val(n) < 0)
  {
    eth_unref(file);
    eth_unref(n);
    return eth_exn(eth_invalid_argument());
  }

  FILE *stream = eth_get_file_stream(file);
  size_t size = eth_num_val(n);
  char *buf = malloc(size + 1);
  size_t nrd = fread(buf, 1, size, stream);
  if (nrd == 0)
  {
    if (feof(stream))
    {
      eth_unref(file);
      eth_unref(n);
      return eth_exn(eth_sym("End_of_file"));
    }
    else if (ferror(stream))
    {
      eth_unref(file);
      eth_unref(n);
      return eth_system_error(0);
    }
  }
  eth_unref(file);
  eth_unref(n);
  buf[nrd] = '\0';
  return eth_create_string_from_ptr2(buf, nrd);
}

static eth_t
_read_file(void)
{
  eth_t file = *eth_sp++;
  if (file->type != eth_file_type)
  {
    eth_drop(file);
    return eth_exn(eth_type_error());
  }

  FILE *stream = eth_get_file_stream(file);

  errno = 0;
  long start = ftell(stream);
  if (errno) goto error;
  fseek(stream, 0, SEEK_END);
  if (errno) goto error;
  long end = ftell(stream);
  if (errno) goto error;
  fseek(stream, start, SEEK_SET);
  if (errno) goto error;

  char *buf = malloc(end - start + 1);
  size_t nrd = fread(buf, 1, end - start, stream);
  if (nrd == 0)
  {
    if (feof(stream))
    {
      eth_drop(file);
      return eth_exn(eth_sym("End_of_file"));
    }
    else if (ferror(stream))
    {
      eth_drop(file);
      return eth_system_error(0);
    }
  }
  eth_drop(file);
  buf[nrd] = '\0';
  return eth_create_string_from_ptr2(buf, nrd);

error:;
  int err = errno;
  eth_drop(file);
  switch (err)
  {
    case EINVAL:
    case ESPIPE:
    case EBADF:
      return eth_exn(eth_invalid_argument());
    default:
      return eth_system_error(err);
  }
}

static eth_t
_tell(void)
{
  eth_args args = eth_start(1);
  eth_t file = eth_arg2(args, eth_file_type);
  int pos = ftell(eth_get_file_stream(file));
  int err = errno;
  if (pos < 0)
  {
    switch (err)
    {
      case EINVAL:
      case ESPIPE:
      case EBADF:
        eth_return(args, eth_exn(eth_invalid_argument()));
      default:
        eth_return(args, eth_exn(eth_sym("System_error")));
    }
  }
  eth_return(args, eth_num(pos));
}

static eth_t
_seek(void)
{
  eth_args args = eth_start(3);
  eth_t file = eth_arg2(args, eth_file_type);
  eth_t whence = eth_arg2(args, eth_number_type);
  eth_t offs = eth_arg2(args, eth_number_type);
  int ret = fseek(eth_get_file_stream(file), eth_num_val(whence), eth_num_val(offs));
  int err = errno;
  if (ret)
  {
    switch (err)
    {
      case EINVAL:
      case ESPIPE:
      case EBADF:
        eth_return(args, eth_exn(eth_invalid_argument()));
      default:
        eth_return(args, eth_exn(eth_sym("System_error")));
    }
  }
  eth_return(args, eth_nil);
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
//                                  printf
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
_fmt(char *fmt, int n, FILE *out)
{
  for (int i = 0; i < n; ++i)
    eth_ref(eth_sp[i]);

  eth_t ret = eth_nil;
  int ipar = 0;
  for (char *p = fmt; *p; ++p)
  {
    switch (*p)
    {
      case '~':
        switch (p[1])
        {
          case 'w':
            eth_write(eth_sp[ipar], out);
            eth_unref(eth_sp[ipar]);
            ipar += 1;
            p += 1;
            break;

          case 'd':
            eth_display(eth_sp[ipar], out);
            eth_unref(eth_sp[ipar]);
            ipar += 1;
            p += 1;
            break;

          case '~':
            putc('~', out);
            p += 1;
            break;

          default:
            assert(!"wtf");
        }
        break;

      default:
        putc(*p, out);
    }
  }

  eth_pop_stack(n);
  return ret;
}

static int
_test_fmt(const char *fmt)
{
  int n = 0;
  const char *p = fmt;
  while (true)
  {
    if ((p = strchr(p, '~')))
    {
      if (p[1] == 'w' || p[1] == 'd')
      {
        n += 1;
        p += 2;
      }
      else if (p[1] == '~')
      {
        p += 2;
      }
      else
      {
        return -1;
      }
      continue;
    }
    return n;
  }
}

static eth_t
_printf_aux(void)
{
  format_data *data = eth_this->proc.data;
  FILE *stream = eth_get_file_stream(data->file);
  char *fmt = ETH_STRING(data->fmt)->cstr;
  int n = data->n;
  return _fmt(fmt, n, stream);
}

static eth_t
_printf(void)
{
  eth_args args = eth_start(2);
  eth_t file = eth_arg2(args, eth_file_type);
  eth_t fmt = eth_arg2(args, eth_string_type);

  int n = _test_fmt(eth_str_cstr(fmt));
  if (n < 0)
    eth_throw(args, eth_sym("Format_error"));

  if (n == 0)
  {
    fputs(ETH_STRING(fmt)->cstr, eth_get_file_stream(file));
    eth_return(args, eth_nil);
  }
  else
  {
    format_data *data = malloc(sizeof(format_data));
    data->fmt = fmt;
    data->file = file;
    data->n = n;
    eth_pop_stack(2);
    return eth_create_proc(_printf_aux, n, data, (void*)destroy_format_data);
  }
}

static eth_t
_format_aux(void)
{
  format_data *data = eth_this->proc.data;
  char *fmt = ETH_STRING(data->fmt)->cstr;
  int n = data->n;

  char *ptr = NULL;
  size_t size = 0;
  FILE *out = open_memstream(&ptr, &size);
  eth_t ret = _fmt(fmt, n, out);
  fclose(out);

  if (ret->type == eth_exception_type)
  {
    free(ptr);
    return ret;
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

  int n = _test_fmt(eth_str_cstr(fmt));
  if (n < 0)
  {
    eth_drop(fmt);
    return eth_exn(eth_sym("Format_error"));
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
static eth_t
_load(void)
{
  static int cnt = 1;
  eth_use_tuple_as(tup2, 2);

  eth_t path = *eth_sp++;
  eth_t env = *eth_sp++;
  if (eth_unlikely(not eth_is_str(path)))
  {
    eth_drop_2(path, env);
    return eth_exn(eth_type_error());
  }

  eth_module *envmod = eth_create_module("");
  for (eth_t it = env; eth_is_pair(it); it = eth_cdr(it))
  {
    eth_t def = eth_car(it);
    if (def->type != tup2)
    {
      eth_drop_2(path, env);
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

  char id[42];
  sprintf(id, "load.%d", cnt++);

  eth_module *mod = eth_create_module(id);
  eth_t ret;
  int ok = eth_load_module_from_script2(g_env, g_env, mod, eth_str_cstr(path),
      &ret, envmod);
  eth_destroy_module(envmod);
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
  eth_remove_module(g_env, id);
  eth_destroy_module(mod);
  return ret;
}

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
_Lazy_create(void)
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
//                                 module
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/*
 * NOTE: IR-builder will use builtins ...while loading builtins.
 */
void
_eth_init_builtins(void)
{
  char buf[PATH_MAX];

  g_env = eth_create_empty_env();
  if (eth_get_prefix())
  {
    sprintf(buf, "%s/lib/ether", eth_get_prefix());
    eth_add_module_path(g_env, buf);
  }

  g_mod = eth_create_module("Builtins");

#if not defined(ETHER_DISABLE_BUILTINS)
  eth_debug("loading builtins");

  eth_attr *attr = eth_create_attr(ETH_ATTR_BUILTIN);

  eth_define_attr(g_mod,    "&&", eth_create_proc(_and, 2, NULL, NULL), attr);
  eth_define_attr(g_mod,    "||", eth_create_proc( _or, 2, NULL, NULL), attr);
  // TODO: `++` should be an instaruction
  eth_define(g_mod,         "++", eth_create_proc(    _strcat, 2, NULL, NULL));
  // ---
  eth_define(g_mod,      "pair?", eth_create_proc(    _pair_p, 1, NULL, NULL));
  eth_define(g_mod,    "symbol?", eth_create_proc(  _symbol_p, 1, NULL, NULL));
  eth_define(g_mod,    "number?", eth_create_proc(  _number_p, 1, NULL, NULL));
  eth_define(g_mod,    "string?", eth_create_proc(  _string_p, 1, NULL, NULL));
  eth_define(g_mod,   "boolean?", eth_create_proc( _boolean_p, 1, NULL, NULL));
  eth_define(g_mod,  "function?", eth_create_proc(_function_p, 1, NULL, NULL));
  eth_define(g_mod,     "tuple?", eth_create_proc(   _tuple_p, 1, NULL, NULL));
  eth_define(g_mod,    "record?", eth_create_proc(  _record_p, 1, NULL, NULL));
  eth_define(g_mod,      "file?", eth_create_proc(    _file_p, 1, NULL, NULL));
  eth_define(g_mod,    "regexp?", eth_create_proc(  _regexp_p, 1, NULL, NULL));
  // ---
  eth_define(g_mod,       "list", eth_create_proc(      _list, 1, NULL, NULL));
  // ---
  eth_define(g_mod,       "dump", eth_create_proc(      _dump, 1, NULL, NULL));
  // ---
  eth_define(g_mod,         "=~", eth_create_proc( _regexp_eq, 2, NULL, NULL));
  // ---
  eth_define(g_mod, "__inclusive_range", eth_create_proc(_inclusive_range, 2, NULL, NULL));
  eth_define(g_mod, "__List_map", eth_create_proc(_map, 2, NULL, NULL));
  eth_define(g_mod, "__List_filter_map", eth_create_proc(_filter_map, 2, NULL, NULL));
  // ---
  eth_define(g_mod,      "stdin", eth_stdin);
  eth_define(g_mod,     "stdout", eth_stdout);
  eth_define(g_mod,     "stderr", eth_stderr);
  eth_define(g_mod,     "__open", eth_create_proc(      _open, 2, NULL, NULL));
  eth_define(g_mod,    "__popen", eth_create_proc(     _popen, 2, NULL, NULL));
  eth_define(g_mod,      "close", eth_create_proc(     _close, 1, NULL, NULL));
  eth_define(g_mod,"read_line_of",eth_create_proc(_read_line_of,1,NULL, NULL));
  eth_define(g_mod,    "read_of", eth_create_proc(   _read_of, 2, NULL, NULL));
  eth_define(g_mod,  "read_file", eth_create_proc( _read_file, 1, NULL, NULL));
  eth_define(g_mod,   "write_to", eth_create_proc(  _write_to, 2, NULL, NULL));
  eth_define(g_mod,    "newline", eth_create_proc(   _newline, 0, NULL, NULL));
  eth_define(g_mod,      "print", eth_create_proc(     _print, 1, NULL, NULL));
  eth_define(g_mod,      "input", eth_create_proc(     _input, 1, NULL, NULL));
  eth_define(g_mod,       "tell", eth_create_proc(      _tell, 1, NULL, NULL));
  eth_define(g_mod,     "__seek", eth_create_proc(      _seek, 3, NULL, NULL));
  // ---
  eth_define(g_mod,   "__system", eth_create_proc(    _system, 1, NULL, NULL));
  // ---
  eth_define(g_mod,   "__printf", eth_create_proc(    _printf, 2, NULL, NULL));
  eth_define(g_mod,     "format", eth_create_proc(    _format, 1, NULL, NULL));
  // ---
  eth_define(g_mod,      "raise", eth_create_proc(     _raise, 1, NULL, NULL));
  eth_define(g_mod,       "exit", eth_create_proc(     __exit, 1, NULL, NULL));
  // ---
  eth_define(g_mod, "__builtin_load", eth_create_proc(_load, 2, NULL, NULL));
  // ---
  eth_define(g_mod, "__Lazy_create", eth_create_proc(_Lazy_create, 1, NULL, NULL));


  if (not eth_resolve_path(g_env, "__builtins.eth", buf))
  {
    eth_error("can't find builtins");
    abort();
  }
  eth_debug("- loading \"%s\"", buf);
  if (not eth_load_module_from_script(g_env, g_env, g_mod, buf, NULL))
  {
    eth_error("failed to load builtins");
    abort();
  }
  eth_debug("builtins were succesfully loaded");
#else
  eth_add_module(g_env, g_mod); // to prevent memory leak
#endif
}

void
_eth_cleanup_builtins(void)
{
  eth_destroy_env(g_env);
}

const eth_module*
eth_builtins(void)
{
  return g_mod;
}

eth_t
eth_get_builtin(const char *name)
{
  eth_def *def = eth_find_def(g_mod, name);
  if (def)
    return def->val;
  else
    return NULL;
}

