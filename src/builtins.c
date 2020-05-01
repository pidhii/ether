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

static eth_t
_match(void)
{
  eth_use_symbol(Regexp_error)

  eth_args args = eth_start(2);
  eth_t reg = eth_arg2(args, eth_regexp_type);
  eth_t str = eth_arg2(args, eth_string_type);
  int n = eth_exec_regexp(reg, eth_str_cstr(str), eth_str_len(str), 0);
  if (n < 0)
    eth_return(args, eth_false);
  else if (n == 0)
    eth_throw(args, Regexp_error);
  else
  {
    eth_t acc = eth_nil;
    const int *ovec = eth_ovector();
    for (int cnt = n - 1; cnt >= 0; --cnt)
    {
      int i = cnt * 2;
      char *p = eth_str_cstr(str) + ovec[i];
      int len = ovec[i+1] - ovec[i];
      eth_t s = eth_create_string2(p, len);
      acc = eth_cons(s, acc);
    }
    eth_return(args, acc);
  }
}

static eth_t
_gsub(void)
{
  eth_use_symbol(Regexp_error)
  eth_use_symbol(Invalid_argument)
  eth_use_symbol(Type_error)

  eth_args args = eth_start(3);
  eth_t reg = eth_arg2(args, eth_regexp_type);
  eth_t f = eth_arg2(args, eth_function_type);
  eth_t str = eth_arg2(args, eth_string_type);

  char *ptr = NULL;
  size_t size = 0;
  FILE *buf = open_memstream(&ptr, &size);

  const int *ovec = eth_ovector();
  const char *p = eth_str_cstr(str);
  const char *pend = p + eth_str_len(str);
  while (true)
  {
    int n = eth_exec_regexp(reg, p, pend - p, 0);
    if (n < 0)
    {
      fwrite(p, 1, pend - p, buf);
      break;
    }
    else if (n == 0)
    {
      fclose(buf);
      free(ptr);
      eth_throw(args, Regexp_error);
    }
    else
    {
      if (ovec[0] == 0 && ovec[1] == 0)
      {
        fclose(buf);
        free(ptr);
        eth_throw(args, Invalid_argument);
      }
      else
      {
        fwrite(p, 1, ovec[0], buf);
        // --
        eth_t x = eth_create_string2(p + ovec[0], ovec[1] - ovec[0]);
        eth_reserve_stack(1);
        eth_sp[0] = x;
        eth_t r = eth_apply(f, 1);
        // --
        if (eth_unlikely(eth_is_exn(r)))
        {
          fclose(buf);
          free(ptr);
          eth_rethrow(args, r);
        }
        if (eth_unlikely(not eth_is_str(r)))
        {
          fclose(buf);
          free(ptr);
          eth_drop(r);
          eth_throw(args, Type_error);
        }
        fwrite(eth_str_cstr(r), 1, eth_str_len(r), buf);
        eth_drop(r);
        p += ovec[1];
      }
    }
  }

  fclose(buf);
  eth_t ret = eth_create_string_from_ptr2(ptr, size);
  eth_return(args, ret);
}

static eth_t
_rev_split(void)
{
  eth_use_symbol(Regexp_error)
  eth_use_symbol(Invalid_argument)

  eth_args args = eth_start(2);
  eth_t reg = eth_arg2(args, eth_regexp_type);
  eth_t str = eth_arg2(args, eth_string_type);

  eth_t acc = eth_nil;
  const int *ovec = eth_ovector();
  const char *p = eth_str_cstr(str);
  const char *pend = p + eth_str_len(str);
  while (true)
  {
    int n = eth_exec_regexp(reg, p, pend - p, 0);
    if (n < 0)
    {
      acc = eth_cons(eth_create_string2(p, pend - p), acc);
      eth_return(args, acc);
    }
    else if (n == 0)
    {
      eth_drop(acc);
      eth_throw(args, Regexp_error);
    }
    else
    {
      if (ovec[0] == 0 && ovec[1] == 0)
      {
        eth_drop(acc);
        eth_throw(args, Invalid_argument);
      }
      else
      {
        acc = eth_cons(eth_create_string2(p, ovec[0]), acc);
        p += ovec[1];
      }
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
_strncmp(void)
{
  eth_t x = *eth_sp++;
  eth_ref(x);
  eth_t y = *eth_sp++;
  eth_ref(y);
  eth_t n = *eth_sp++;
  if (eth_unlikely(not eth_is_str(x) or not eth_is_str(y) or not eth_is_num(n)))
  {
    eth_unref(x);
    eth_unref(y);
    eth_unref(n);
    return eth_exn(eth_sym("Type_error"));
  }
  if (eth_unlikely(not eth_is_size(n)))
  {
    eth_unref(x);
    eth_unref(y);
    eth_unref(n);
    return eth_exn(eth_sym("Invalid_argument"));
  }
  int cmp = strncmp(eth_str_cstr(x), eth_str_cstr(y), eth_num_val(n));
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
_strncasecmp(void)
{
  eth_t x = *eth_sp++;
  eth_ref(x);
  eth_t y = *eth_sp++;
  eth_ref(y);
  eth_t n = *eth_sp++;
  if (eth_unlikely(not eth_is_str(x) or not eth_is_str(y) or not eth_is_num(n)))
  {
    eth_unref(x);
    eth_unref(y);
    eth_unref(n);
    return eth_exn(eth_sym("Type_error"));
  }
  if (eth_unlikely(not eth_is_size(n)))
  {
    eth_unref(x);
    eth_unref(y);
    eth_unref(n);
    return eth_exn(eth_sym("Invalid_argument"));
  }
  int cmp = strncasecmp(eth_str_cstr(x), eth_str_cstr(y), eth_num_val(n));
  eth_unref(x);
  eth_unref(y);
  return eth_num(cmp);
}

static eth_t
_substr(void)
{
  eth_args args = eth_start(3);
  eth_t str = eth_arg2(args, eth_string_type);
  eth_t start = eth_arg2(args, eth_number_type);
  eth_t len = eth_arg2(args, eth_number_type);
  if (eth_unlikely(not eth_is_size(start) || not eth_is_size(len)))
    eth_throw(args, eth_sym("Invalid_argument"));
  // --
  char *s = eth_str_cstr(str);
  size_t slen = eth_str_len(str);
  size_t at = eth_num_val(start);
  size_t n = eth_num_val(len);
  if (at + n > slen)
    eth_throw(args, eth_sym("Out_of_range"));
  eth_unref(start);
  eth_unref(len);
  eth_dec(str);
  eth_pop_stack(3);
  // --
  if (str->rc == 0)
  { // mutate supplied string
    if (at >= n)
      // non-overlapping regions => safe to use memcpy()
      memcpy(s, s + at, n);
    else
    { // handle overlaping regions
      for (size_t i = 0; i < n; ++i)
        s[i] = s[at + i];
    }
    s[n] = '\0';
    ETH_STRING(str)->len = n;
    return str;
  }
  else
    return eth_create_string2(s + at, n);
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
      return eth_exn(eth_sym("Type_error"));
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
    return eth_exn(eth_sym("Invalid_argument"));
  }
  cod_vec_push(buf, '\0');
  return eth_create_string_from_ptr2(buf.data, buf.len - 1);
}

static eth_t
_strstr(void)
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
  char *p = strstr(eth_str_cstr(x), eth_str_cstr(y));
  eth_t ret = p ? eth_num(p - eth_str_cstr(x)) : eth_false;
  eth_unref(x);
  eth_unref(y);
  return ret;
}

#ifdef _GNU_SOURCE
static eth_t
_strcasestr(void)
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
  char *p = strcasestr(eth_str_cstr(x), eth_str_cstr(y));
  eth_t ret = p ? eth_num(p - eth_str_cstr(x)) : eth_false;
  eth_unref(x);
  eth_unref(y);
  return ret;
}
#endif

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

static eth_t
_ord(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_str(x)))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }
  if (eth_unlikely(eth_str_len(x) != 1))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Invalid_argument"));
  }
  eth_t ret = eth_num(eth_str_cstr(x)[0]);
  eth_drop(x);
  return ret;
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
    return eth_exn(eth_sym("Invalid_argument"));
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
    return eth_exn(eth_sym("Invalid_argument"));
  }
  eth_ref(acc);
  eth_unref(l);
  eth_dec(acc);
  return acc;
}

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
_rev_map(void)
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

  eth_return(args, acc);
}

static eth_t
_rev_mapi(void)
{
  eth_use_symbol(Improper_list)

  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t l = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it;
  eth_number_t i = 0;
  for (it = l; eth_is_pair(it); it = eth_cdr(it), ++i)
  {
    eth_reserve_stack(2);
    eth_sp[0] = eth_num(i);
    eth_sp[1] = eth_car(it);
    const eth_t v = eth_apply(f, 2);
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

  eth_return(args, acc);
}

static eth_t
_rev_zip(void)
{
  eth_use_symbol(Improper_list)

  eth_args args = eth_start(3);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t xs = eth_arg(args);
  const eth_t ys = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it1, it2;
  for (it1 = xs, it2 = ys; eth_is_pair(it1) and eth_is_pair(it2);
      it1 = eth_cdr(it1), it2 = eth_cdr(it2))
  {
    eth_reserve_stack(2);
    eth_sp[0] = eth_car(it1);
    eth_sp[1] = eth_car(it2);
    const eth_t v = eth_apply(f, 2);
    if (eth_unlikely(eth_is_exn(v)))
    {
      eth_drop(acc);
      eth_rethrow(args, v);
    }
    acc = eth_cons(v, acc);
  }
  if (eth_unlikely(not eth_is_pair(it1) and it1 != eth_nil or
                   not eth_is_pair(it2) and it2 != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, Improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_zipi(void)
{
  eth_use_symbol(Improper_list)

  eth_args args = eth_start(3);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t xs = eth_arg(args);
  const eth_t ys = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it1, it2;
  eth_number_t i = 0;
  for (it1 = xs, it2 = ys; eth_is_pair(it1) and eth_is_pair(it2);
      it1 = eth_cdr(it1), it2 = eth_cdr(it2), ++i)
  {
    eth_reserve_stack(3);
    eth_sp[0] = eth_num(i);
    eth_sp[1] = eth_car(it1);
    eth_sp[2] = eth_car(it2);
    const eth_t v = eth_apply(f, 3);
    if (eth_unlikely(eth_is_exn(v)))
    {
      eth_drop(acc);
      eth_rethrow(args, v);
    }
    acc = eth_cons(v, acc);
  }
  if (eth_unlikely(not eth_is_pair(it1) and it1 != eth_nil or
                   not eth_is_pair(it2) and it2 != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, Improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_filter_map(void)
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

  eth_return(args, acc);
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
      case EINVAL: exn = eth_exn(eth_sym("Invalid_argument")); break;
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
      case EINVAL: exn = eth_exn(eth_sym("Invalid_argument")); break;
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
          case EBADF: return eth_exn(eth_sym("Invalid_argument"));
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
        eth_throw(args, eth_sym("Invalid_argument"));
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
        case EINVAL: return eth_exn(eth_sym("Invalid_argument"));
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
    return eth_exn(eth_sym("Type_error"));
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
        case EINVAL: return eth_exn(eth_sym("Invalid_argument"));
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
    return eth_exn(eth_sym("Type_error"));
  }
  if (eth_num_val(n) < 0)
  {
    eth_unref(file);
    eth_unref(n);
    return eth_exn(eth_sym("Invalid_argument"));
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
    return eth_exn(eth_sym("Type_error"));
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
      return eth_exn(eth_sym("Invalid_argument"));
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
        eth_return(args, eth_exn(eth_sym("Invalid_argument")));
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
        eth_return(args, eth_exn(eth_sym("Invalid_argument")));
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
  eth_use_symbol(Invalid_argument)

  eth_args args = eth_start(1);
  eth_t status = eth_arg2(args, eth_number_type);
  if (not eth_is_int(status))
    eth_throw(args, Invalid_argument);
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

  eth_t path = *eth_sp++;
  if (eth_unlikely(not eth_is_str(path)))
  {
    eth_drop(path);
    return eth_exn(eth_sym("Type_error"));
  }

  char id[42];
  sprintf(id, "load.%d", cnt++);

  eth_module *mod = eth_create_module(id);
  eth_t ret;
  if (not eth_load_module_from_script(g_env, mod, eth_str_cstr(path), &ret))
  {
    eth_drop(path);
    eth_destroy_module(mod);
    return eth_exn(eth_sym("Failure"));
  }

  eth_t acc = eth_nil;
  int ndefs = eth_get_ndefs(mod);
  eth_def defs[ndefs];
  eth_get_defs(mod, defs);
  for (int i = 0; i < ndefs; ++i)
    acc = eth_cons(eth_tup2(eth_str(defs[i].ident), defs[i].val), acc);

  ret = eth_tup2(ret, acc);
  eth_drop(path);
  eth_remove_module(g_env, id);
  eth_destroy_module(mod);
  return ret;
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
  // ---
  eth_define(g_mod,  "to_number", eth_create_proc( _to_number, 1, NULL, NULL));
  eth_define(g_mod,  "to_symbol", eth_create_proc( _to_symbol, 1, NULL, NULL));
  eth_define(g_mod,       "list", eth_create_proc(      _list, 1, NULL, NULL));
  // ---
  eth_define(g_mod,       "dump", eth_create_proc(      _dump, 1, NULL, NULL));
  // ---
  eth_define(g_mod,     "strlen", eth_create_proc(    _strlen, 1, NULL, NULL));
  eth_define(g_mod,   "to_upper", eth_create_proc(  _to_upper, 1, NULL, NULL));
  eth_define(g_mod,   "to_lower", eth_create_proc(  _to_lower, 1, NULL, NULL));
  eth_define(g_mod,     "strcmp", eth_create_proc(    _strcmp, 2, NULL, NULL));
  eth_define(g_mod, "strcasecmp", eth_create_proc(_strcasecmp, 2, NULL, NULL));
  eth_define(g_mod,    "strncmp", eth_create_proc(   _strncmp, 3, NULL, NULL));
  eth_define(g_mod,"strncasecmp",eth_create_proc(_strncasecmp, 3, NULL, NULL));
  eth_define(g_mod,   "__substr", eth_create_proc(    _substr, 3, NULL, NULL));
  eth_define(g_mod,     "strstr", eth_create_proc(    _strstr, 2, NULL, NULL));
#ifdef _GNU_SOURCE
  eth_define(g_mod, "strcasestr", eth_create_proc(_strcasestr, 2, NULL, NULL));
#endif
  eth_define(g_mod,     "concat", eth_create_proc(    _concat, 1, NULL, NULL));
  eth_define(g_mod,      "chomp", eth_create_proc(     _chomp, 1, NULL, NULL));
  eth_define(g_mod,       "chop", eth_create_proc(      _chop, 1, NULL, NULL));
  eth_define(g_mod,        "chr", eth_create_proc(       _chr, 1, NULL, NULL));
  eth_define(g_mod,        "ord", eth_create_proc(       _ord, 1, NULL, NULL));
  // ---
  eth_define(g_mod,         "=~", eth_create_proc( _regexp_eq, 2, NULL, NULL));
  eth_define(g_mod,      "match", eth_create_proc(     _match, 2, NULL, NULL));
  eth_define(g_mod,       "gsub", eth_create_proc(      _gsub, 3, NULL, NULL));
  eth_define(g_mod,  "rev_split", eth_create_proc( _rev_split, 2, NULL, NULL));
  // ---
  eth_define(g_mod, "__List_length", eth_create_proc(_length, 1, NULL, NULL));
  eth_define(g_mod, "__List_rev_append", eth_create_proc(_rev_append, 2, NULL, NULL));
  eth_define(g_mod, "__inclusive_range", eth_create_proc(_inclusive_range, 2, NULL, NULL));
  eth_define(g_mod, "__List_rev_map", eth_create_proc(   _rev_map, 2, NULL, NULL));
  eth_define(g_mod, "__List_rev_mapi", eth_create_proc(  _rev_mapi, 2, NULL, NULL));
  eth_define(g_mod, "__List_rev_zip", eth_create_proc(   _rev_zip, 3, NULL, NULL));
  eth_define(g_mod, "__List_rev_zipi", eth_create_proc(  _rev_zipi, 3, NULL, NULL));
  eth_define(g_mod, "__List_rev_filter_map", eth_create_proc(_rev_filter_map, 2, NULL, NULL));
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
  eth_define(g_mod,       "load", eth_create_proc(      _load, 1, NULL, NULL));


  if (not eth_resolve_path(g_env, "__builtins.eth", buf))
  {
    eth_error("can't find builtins");
    abort();
  }
  eth_debug("- loading \"%s\"", buf);
  if (not eth_load_module_from_script(g_env, g_mod, buf, NULL))
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

