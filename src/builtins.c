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
_to_number(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }

  char *nptr = eth_str_cstr(x);
  char *endptr;
  eth_number_t ret = eth_strtonum(nptr, &endptr);

  if (endptr == nptr)
  {
    eth_drop(x);
    return eth_exn(eth_sym("Failure"));
  }

  while (*endptr++)
  {
    if (not isspace(*endptr))
    {
      eth_drop(x);
      return eth_exn(eth_sym("Failure"));
    }
  }

  eth_drop(x);
  return eth_num(ret);
}

static eth_t
_to_symbol(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }

  eth_t ret = eth_sym(eth_str_cstr(x));
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
    return eth_exn(eth_sym("Failure"));
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  string
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_strlen(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }
  eth_t ret = eth_num(eth_str_len(x));
  eth_drop(x);
  return ret;
}

static eth_t
_to_upper(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }
  char *s = eth_str_cstr(x);
  int len = eth_str_len(x);
  if (x->rc == 0)
  {
    for (int i = 0; i < len; ++i)
      s[i] = toupper(s[i]);
    return x;
  }
  else
  {
    char *buf = malloc(len + 1);
    for (int i = 0; i < len; ++i)
      buf[i] = toupper(s[i]);
    buf[len] = '\0';
    return eth_create_string_from_ptr2(buf, len);
  }
}

static eth_t
_to_lower(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }
  char *s = eth_str_cstr(x);
  int len = eth_str_len(x);
  if (x->rc == 0)
  {
    for (int i = 0; i < len; ++i)
      s[i] = tolower(s[i]);
    return x;
  }
  else
  {
    char *buf = malloc(len + 1);
    for (int i = 0; i < len; ++i)
      buf[i] = tolower(s[i]);
    buf[len] = '\0';
    return eth_create_string_from_ptr2(buf, len);
  }
}

static eth_t
_strcmp(void)
{
  eth_t x = *eth_sp++;
  eth_ref(x);
  eth_t y = *eth_sp++;
  eth_ref(y);
  if (eth_unlikely(not eth_is_str(x) || not eth_is_str(y)))
  {
    eth_unref(x);
    eth_unref(y);
    return eth_exn(eth_sym("Type_error"));
  }
  int cmp = strcmp(eth_str_cstr(x), eth_str_cstr(y));
  eth_unref(x);
  eth_unref(y);
  return eth_num(cmp);
}

static eth_t
_strcasecmp(void)
{
  eth_t x = *eth_sp++;
  eth_ref(x);
  eth_t y = *eth_sp++;
  eth_ref(y);
  if (eth_unlikely(not eth_is_str(x) || not eth_is_str(y)))
  {
    eth_unref(x);
    eth_unref(y);
    return eth_exn(eth_sym("Type_error"));
  }
  int cmp = strcasecmp(eth_str_cstr(x), eth_str_cstr(y));
  eth_unref(x);
  eth_unref(y);
  return eth_num(cmp);
}

static eth_t
_concat(void)
{
  eth_t x = *eth_sp++;

  cod_vec(char) buf;
  cod_vec_init(buf);

  eth_t it;
  for (it = x; eth_is_pair(it); it = eth_cdr(it))
  {
    eth_t s = eth_car(it);
    if (eth_unlikely(not eth_is_str(s)))
    {
      eth_drop(x);
      cod_vec_destroy(buf);
      return eth_exn(eth_sym("Invalid_argument"));
    }
    int len = eth_str_len(s);
    char *cstr = eth_str_cstr(s);
    for (int i = 0; i < len; ++i)
      cod_vec_push(buf, cstr[i]);
  }
  eth_drop(x);
  if (eth_unlikely(it != eth_nil))
  {
    cod_vec_destroy(buf);
    return eth_exn(eth_sym("Improper_list"));
  }
  cod_vec_push(buf, '\0');
  return eth_create_string_from_ptr2(buf.data, buf.len - 1);
}

static eth_t
_chomp(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_str(x)))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }
  int len = eth_str_len(x);
  if (len == 0) return x;
  char *str = eth_str_cstr(x);
  while (len > 0 && str[len-1] == '\n') len -= 1;
  if (x->rc == 0)
  {
    str[len] = '\0';
    ETH_STRING(x)->len = len;
    return x;
  }
  else
  {
    eth_t ret = eth_create_string2(eth_str_cstr(x), len);
    return ret;
  }
}

static eth_t
_chop(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_str(x)))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }
  if (eth_str_len(x) == 0)
    return x;
  if (x->rc == 0)
  {
    int len = ETH_STRING(x)->len -= 1;
    ETH_STRING(x)->cstr[len] = '\0';
    return x;
  }
  else
  {
    eth_t ret = eth_create_string2(eth_str_cstr(x), eth_str_len(x) - 1);
    return ret;
  }
}

static eth_t
_chr(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_num(x)))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }
  eth_number_t num = eth_num_val(x);
  eth_drop(x);
  if (num < CHAR_MIN or num > UINT8_MAX)
    return eth_exn(eth_sym("Invalid_argument"));
  else
    return eth_create_string_from_char((char)num);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  lists
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_length(void)
{
  eth_t l = *eth_sp++;
  bool isproper;
  size_t len = eth_length(l, &isproper);
  eth_drop(l);
  if (isproper)
    return eth_num(len);
  else
    return eth_exn(eth_sym("Improper_list"));
}

static eth_t
_rev_append(void)
{
  eth_t l = *eth_sp++;
  eth_ref(l);

  eth_t acc = *eth_sp++;

  eth_t it = l;
  for (; it->type == eth_pair_type; it = eth_cdr(it))
    acc = eth_cons(eth_car(it), acc);

  if (it != eth_nil)
  {
    eth_drop(acc);
    eth_unref(l);
    return eth_exn(eth_sym("Improper_list"));
  }
  eth_ref(acc);
  eth_unref(l);
  eth_dec(acc);
  return acc;
}
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                             list comprehension
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/*static eth_t*/
/*lc_filter_map_1(eth_t fn, eth_t l, eth_t p)*/
/*{*/
  /*cod_vec(eth_t) buf;*/
  /*cod_vec_init_with_cap(buf, 0x80);*/
  /*eth_t exn;*/
  /*eth_t acc;*/

  /*if (p == eth_true)*/
  /*{*/
    /*for (eth_t it = l; it->type == eth_pair_type; it = eth_cdr(it))*/
    /*{*/
      /*eth_t x = eth_car(it);*/
      /**--eth_sp = x;*/
      /*x = _eth_raw_apply(fn, 1);*/
      /*if (eth_unlikely(x->type == eth_exception_type))*/
      /*{*/
        /*exn = x;*/
        /*goto error;*/
      /*}*/
      /*// ---*/
      /*eth_ref(x);*/
      /*cod_vec_push(buf, x);*/
    /*}*/
  /*}*/
  /*else*/
  /*{*/
    /*for (eth_t it = l; it->type == eth_pair_type; it = eth_cdr(it))*/
    /*{*/
      /*eth_t tmp;*/

      /*eth_t x = eth_car(it);*/
      /*eth_ref(x);*/
      /*// ---*/
      /**--eth_sp = x;*/
      /*tmp = _eth_raw_apply(p, 1);*/
      /*if (eth_unlikely(tmp->type == eth_exception_type))*/
      /*{*/
        /*exn = tmp;*/
        /*goto error;*/
      /*}*/
      /*if (tmp == eth_false)*/
      /*{*/
        /*eth_unref(x);*/
        /*continue;*/
      /*}*/
      /*eth_drop(tmp);*/
      /*// ---*/
      /**--eth_sp = x;*/
      /*eth_dec(x);*/
      /*x = _eth_raw_apply(fn, 1);*/
      /*if (eth_unlikely(x->type == eth_exception_type))*/
      /*{*/
        /*exn = x;*/
        /*goto error;*/
      /*}*/
      /*// ---*/
      /*eth_ref(x);*/
      /*cod_vec_push(buf, x);*/
    /*}*/
  /*}*/

  /*acc = eth_nil;*/
  /*for (int i = (int)buf.len - 1; i >= 0; --i)*/
  /*{*/
    /*eth_ref(acc);*/
    /*acc = eth_cons_noref(buf.data[i], acc);*/
  /*}*/
  /*cod_vec_destroy(buf);*/
  /*return acc;*/

/*error:*/
  /*cod_vec_iter(buf, i, x, eth_unref(x));*/
  /*cod_vec_destroy(buf);*/
  /*return exn;*/
/*}*/

/*static eth_t*/
/*_lc_filter_map(void)*/
/*{*/
  /*eth_t fn = eth_sp[0];*/
  /*eth_ref(fn);*/
  /*assert(fn->type == eth_function_type);*/

  /*eth_t ls = eth_sp[1];*/
  /*eth_ref(ls);*/

  /*eth_t p = eth_sp[2];*/
  /*eth_ref(p);*/

  /*eth_t ret;*/
  /*if (eth_is_tuple(ls->type))*/
  /*{*/
    /*int nl = eth_tuple_size(ls->type);*/
    /*assert(ETH_FUNCTION(fn)->arity == nl);*/
    /*assert(*/
        /*(p->type == eth_function_type && ETH_FUNCTION(p)->arity == nl)*/
        /*|| p == eth_true);*/
    /*eth_error("unimplemented");*/
    /*abort();*/
  /*}*/
  /*else*/
  /*{*/
    /*assert(ETH_FUNCTION(fn)->arity == 1);*/
    /*assert(*/
        /*(p->type == eth_function_type && ETH_FUNCTION(p)->arity == 1)*/
        /*|| p == eth_true);*/
    /*ret = lc_filter_map_1(fn, ls, p);*/
  /*}*/

  /*eth_unref(fn);*/
  /*eth_unref(ls);*/
  /*eth_unref(p);*/
  /*eth_pop_stack(3);*/
  /*return ret;*/
/*}*/

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
      case EINVAL: exn = eth_exn(eth_sym("Invalid_argument")); break;
      case ENOMEM: exn = eth_exn(eth_sym("Out_of_memory")); break;
      default: exn = eth_exn(eth_sym("System_error")); break;
    }
    eth_return(args, exn);
  }
  eth_return(args, file);
}

static eth_t
_open_pipe(void)
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
      case EINVAL: exn = eth_exn(eth_sym("Invalid_argument")); break;
      default: exn = eth_exn(eth_sym("System_error")); break;
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
    return eth_exn(eth_sym("Type_error"));
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
        return eth_exn(eth_sym("System_error"));
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
          case EBADF: return eth_exn(eth_sym("Invalid_argument"));
          default: return eth_exn(eth_sym("System_error"));
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
_write(void)
{
  eth_t x = *eth_sp++;
  eth_write(x, stdout);
  eth_drop(x);
  return eth_nil;
}

static eth_t
_display(void)
{
  eth_t x = *eth_sp++;
  eth_display(x, stdout);
  eth_drop(x);
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
        case EINVAL: return eth_exn(eth_sym("Invalid_argument"));
        case ENOMEM: return eth_exn(eth_sym("Out_of_memory"));
        default: abort();
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
    return eth_exn(eth_sym("Type_error"));
  }

  char *line = NULL;
  size_t n = 0;
  ssize_t nrd = getline(&line, &n, eth_get_file_stream(file));
  int err = errno;
  eth_drop(file);

  if (nrd < 0)
  {
    free(line);
    if (feof(eth_get_file_stream(file)))
      return eth_exn(eth_sym("End_of_file"));
    else
    {
      switch (err)
      {
        case EINVAL: return eth_exn(eth_sym("Invalid_argument"));
        case ENOMEM: return eth_exn(eth_sym("Out_of_memory"));
        default: abort();
      }
    }
  }
  return eth_create_string_from_ptr2(line, nrd);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  printf
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  eth_t fmt;
  int n;
} format_data;

static void
destroy_format_data(format_data *data)
{
  eth_unref(data->fmt);
  free(data);
}

static eth_t
_fmt(char *fmt, int n, FILE *out)
{
  for (int i = 0; i < n; ++i)
    eth_ref(eth_sp[i]);

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
  return eth_nil;
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
  char *fmt = ETH_STRING(data->fmt)->cstr;
  int n = data->n;
  return _fmt(fmt, n, stdout);
}

static eth_t
_printf(void)
{
  eth_t fmt = *eth_sp++;
  if (fmt->type != eth_string_type)
  {
    eth_drop(fmt);
    return eth_exn(eth_sym("Type_error"));
  }

  int n = _test_fmt(eth_str_cstr(fmt));
  if (n < 0)
  {
    eth_drop(fmt);
    return eth_exn(eth_sym("Format_error"));
  }

  if (n == 0)
  {
    fputs(ETH_STRING(fmt)->cstr, stdout);
    eth_drop(fmt);
    return eth_nil;
  }
  else
  {
    format_data *data = malloc(sizeof(format_data));
    eth_ref(data->fmt = fmt);
    data->n = n;
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
    return eth_exn(eth_sym("Type_error"));
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

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                 module
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static
eth_env *g_env;

static
eth_module *g_mod;

/*
 * NOTE: IR-builder will use builtins ...while loading builtins.
 */
void
_eth_init_builtins(void)
{
  g_env = eth_create_empty_env();
  g_mod = eth_create_module("Builtins");

  eth_debug("loading builtins");

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
  // ---
  eth_define(g_mod,  "to_number", eth_create_proc( _to_number, 1, NULL, NULL));
  eth_define(g_mod,  "to_symbol", eth_create_proc( _to_symbol, 1, NULL, NULL));
  eth_define(g_mod,       "list", eth_create_proc(      _list, 1, NULL, NULL));
  // ---
  eth_define(g_mod,     "strlen", eth_create_proc(    _strlen, 1, NULL, NULL));
  eth_define(g_mod,   "to_upper", eth_create_proc(  _to_upper, 1, NULL, NULL));
  eth_define(g_mod,   "to_lower", eth_create_proc(  _to_lower, 1, NULL, NULL));
  eth_define(g_mod,     "strcmp", eth_create_proc(    _strcmp, 2, NULL, NULL));
  eth_define(g_mod, "strcasecmp", eth_create_proc(_strcasecmp, 2, NULL, NULL));
  eth_define(g_mod,     "concat", eth_create_proc(    _concat, 1, NULL, NULL));
  eth_define(g_mod,      "chomp", eth_create_proc(     _chomp, 1, NULL, NULL));
  eth_define(g_mod,       "chop", eth_create_proc(      _chop, 1, NULL, NULL));
  eth_define(g_mod,        "chr", eth_create_proc(       _chr, 1, NULL, NULL));
  // ---
  eth_define(g_mod,     "length", eth_create_proc(    _length, 1, NULL, NULL));
  eth_define(g_mod, "rev_append", eth_create_proc(_rev_append, 2, NULL, NULL));
  // ---
  eth_define(g_mod,      "stdin", eth_stdin);
  eth_define(g_mod,     "stdout", eth_stdout);
  eth_define(g_mod,     "stderr", eth_stderr);
  eth_define(g_mod, "__builtin_open", eth_create_proc(_open, 2, NULL, NULL));
  eth_define(g_mod, "__builtin_open_pipe", eth_create_proc(_open_pipe, 2, NULL, NULL));
  eth_define(g_mod,      "close", eth_create_proc(     _close, 1, NULL, NULL));
  eth_define(g_mod, "read_line_of", eth_create_proc(_read_line_of, 1, NULL, NULL));
  eth_define(g_mod,      "write", eth_create_proc(     _write, 1, NULL, NULL));
  eth_define(g_mod,    "display", eth_create_proc(   _display, 1, NULL, NULL));
  eth_define(g_mod,    "newline", eth_create_proc(   _newline, 0, NULL, NULL));
  eth_define(g_mod,      "print", eth_create_proc(     _print, 1, NULL, NULL));
  eth_define(g_mod,      "input", eth_create_proc(     _input, 1, NULL, NULL));
  // ---
  eth_define(g_mod,     "printf", eth_create_proc(    _printf, 1, NULL, NULL));
  eth_define(g_mod,     "format", eth_create_proc(    _format, 1, NULL, NULL));
  // ---
  eth_define(g_mod,      "raise", eth_create_proc(     _raise, 1, NULL, NULL));

  eth_debug("loading \"src/builtins.eth\"");
  if (not eth_load_module_from_script(g_env, g_mod, "src/builtins.eth"))
  {
    eth_error("failed to load builtins");
    abort();
  }
  eth_debug("builtins were succesfully loaded");
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

