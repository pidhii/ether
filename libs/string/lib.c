#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>


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
_cat(void)
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
  eth_use_variant(Some)
  eth_use_symbol(Type_error)
  eth_t x = *eth_sp++;
  eth_ref(x);
  eth_t y = *eth_sp++;
  eth_ref(y);
  if (eth_unlikely(not eth_is_str(x) || not eth_is_str(y)))
  {
    eth_unref(x);
    eth_unref(y);
    return eth_exn(Type_error);
  }
  char *p = strstr(eth_str_cstr(x), eth_str_cstr(y));
  eth_t ret = p ? Some(eth_num(p - eth_str_cstr(x))) : eth_false;
  eth_unref(x);
  eth_unref(y);
  return ret;
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

static eth_t
_find_regexp(void)
{
  eth_use_variant(Some)
  eth_use_symbol(Regexp_error)
  eth_args args = eth_start(2);
  eth_t re = eth_arg2(args, eth_regexp_type);
  eth_t str = eth_arg2(args, eth_string_type);
  const char *p = eth_str_cstr(str);
  int n = eth_exec_regexp(re, p, eth_str_len(str), 0);
  if (n == 0)
    eth_throw(args, Regexp_error);
  else if (n < 0)
    eth_return(args, eth_false);
  else
    eth_return(args, Some(eth_num(eth_ovector()[0])));
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


int
ether_module(eth_module *mod, eth_env *topenv)
{
  eth_define(mod, "len", eth_create_proc(_strlen, 1, NULL, NULL));
  eth_define(mod, "to_upper", eth_create_proc(_to_upper, 1, NULL, NULL));
  eth_define(mod, "to_lower", eth_create_proc(_to_lower, 1, NULL, NULL));
  eth_define(mod, "cmp", eth_create_proc(_strcmp, 2, NULL, NULL));
  eth_define(mod, "casecmp", eth_create_proc(_strcasecmp, 2, NULL, NULL));
  eth_define(mod, "__substr", eth_create_proc(_substr, 3, NULL, NULL));
  eth_define(mod, "__strstr", eth_create_proc(_strstr, 2, NULL, NULL));
  eth_define(mod, "cat", eth_create_proc(_cat, 1, NULL, NULL));
  eth_define(mod, "chomp", eth_create_proc(_chomp, 1, NULL, NULL));
  eth_define(mod, "chop", eth_create_proc(_chop, 1, NULL, NULL));
  eth_define(mod, "chr", eth_create_proc(_chr, 1, NULL, NULL));
  eth_define(mod, "ord", eth_create_proc(_ord, 1, NULL, NULL));
  // --
  eth_define(mod,  "to_number", eth_create_proc( _to_number, 1, NULL, NULL));
  eth_define(mod,  "to_symbol", eth_create_proc( _to_symbol, 1, NULL, NULL));
  // --
  eth_define(mod, "match", eth_create_proc(_match, 2, NULL, NULL));
  eth_define(mod, "gsub", eth_create_proc(_gsub, 3, NULL, NULL));
  eth_define(mod, "rev_split", eth_create_proc(_rev_split, 2, NULL, NULL));
  eth_define(mod, "__find_regexp", eth_create_proc(_find_regexp, 2, NULL, NULL));

  int ret = 0;
  if (not eth_load_module_from_script2(topenv, NULL, mod, "lib.eth", NULL, mod))
    ret = -1;
  return ret;
}

