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
#include <assert.h>
#include <ctype.h>
#include <string.h>


static eth_t
_malloc(void)
{
  eth_t n = *eth_sp++;
  if (eth_unlikely(not eth_is_num(n)))
  {
    eth_drop(n);
    return eth_exn(eth_type_error());
  }
  int len = eth_num_val(n);
  char *str = malloc(len + 1);
  str[len] = '\0';
  eth_drop(n);
  return eth_create_string_from_ptr2(str, len);
}

static eth_t
_calloc(void)
{
  eth_t n = *eth_sp++;
  if (eth_unlikely(not eth_is_num(n)))
  {
    eth_drop(n);
    return eth_exn(eth_type_error());
  }
  int len = eth_num_val(n);
  char *str = calloc(len + 1, sizeof(char));
  str[len] = '\0';
  eth_drop(n);
  return eth_create_string_from_ptr2(str, len);
}


static eth_t
_make(void)
{
  eth_args args = eth_start(2);
  eth_t leng = eth_arg2(args, eth_number_type);
  if (not eth_is_size(leng))
    eth_throw(args, eth_invalid_argument());
  int len = eth_num_val(leng);
  char c = *eth_str_cstr(eth_arg2(args, eth_string_type));
  char *str;
  if (c == 0)
  {
    str = calloc(len + 1, 1);
  }
  else
  {
    str = malloc(len + 1);
    memset(str, c, len);
    str[len] = '\0';
  }
  eth_return(args, eth_create_string_from_ptr2(str, len));
}

static eth_t
_strlen(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
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
    return eth_exn(eth_type_error());
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
    return eth_exn(eth_type_error());
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
    return eth_exn(eth_type_error());
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
    return eth_exn(eth_type_error());
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
    eth_throw(args, eth_invalid_argument());
  // --
  char *s = eth_str_cstr(str);
  size_t slen = eth_str_len(str);
  size_t at = eth_num_val(start);
  size_t n = eth_num_val(len);
  if (at + n > slen)
    eth_throw(args, eth_sym("out_of_range"));
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
      return eth_exn(eth_type_error());
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
    return eth_exn(eth_invalid_argument());
  }
  cod_vec_push(buf, '\0');
  return eth_create_string_from_ptr2(buf.data, buf.len - 1);
}

static eth_t
_strstr_opt(void)
{
  eth_use_variant(some)
  eth_t x = *eth_sp++;
  eth_ref(x);
  eth_t y = *eth_sp++;
  eth_ref(y);
  if (eth_unlikely(not eth_is_str(x) || not eth_is_str(y)))
  {
    eth_unref(x);
    eth_unref(y);
    return eth_exn(eth_type_error());
  }
  char *p = strstr(eth_str_cstr(x), eth_str_cstr(y));
  eth_t ret = p ? some(eth_num(p - eth_str_cstr(x))) : eth_false;
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
    return eth_exn(eth_type_error());
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
    return eth_exn(eth_type_error());
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
_trim_left(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_str(x)))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  char *p = eth_str_cstr(x);
  while (*p and isspace(*p)) ++p;

  if (p == eth_str_cstr(x))
    return x;

  int nwlen = eth_str_len(x) - (p - eth_str_cstr(x));
  if (x->rc == 0)
  {
    memmove(eth_str_cstr(x), p, nwlen + 1);
    ETH_STRING(x)->len = nwlen;
    return x;
  }
  else
    return eth_create_string2(p, nwlen);
}

static eth_t
_trim_right(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_str(x)))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  int len = eth_str_len(x);
  if (len == 0) return x;
  char *str = eth_str_cstr(x);
  while (len > 0 and isspace(str[len-1])) len -= 1;
  if (x->rc == 0)
  {
    str[len] = '\0';
    ETH_STRING(x)->len = len;
    return x;
  }
  else
    return eth_create_string2(eth_str_cstr(x), len);
}

static eth_t
_chr(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_num(x)))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  eth_number_t num = eth_num_val(x);
  eth_drop(x);
  if (num < CHAR_MIN or num > UINT8_MAX)
    return eth_exn(eth_invalid_argument());
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
    return eth_exn(eth_type_error());
  }
  if (eth_unlikely(eth_str_len(x) != 1))
  {
    eth_drop(x);
    return eth_exn(eth_invalid_argument());
  }
  eth_t ret = eth_num(eth_str_cstr(x)[0]);
  eth_drop(x);
  return ret;
}

static eth_t
_match(void)
{
  eth_use_symbol(regexp_error)

  eth_args args = eth_start(2);
  eth_t reg = eth_arg2(args, eth_regexp_type);
  eth_t str = eth_arg2(args, eth_string_type);
  int n = eth_exec_regexp(reg, eth_str_cstr(str), eth_str_len(str), 0);
  if (n < 0)
    eth_return(args, eth_false);
  else if (n == 0)
    eth_throw(args, regexp_error);
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
  eth_use_symbol(regexp_error)

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
    int ovec0 = ovec[0], ovec1 = ovec[1];
    if (n < 0)
    {
      fwrite(p, 1, pend - p, buf);
      break;
    }
    else if (n == 0)
    {
      fclose(buf);
      free(ptr);
      eth_throw(args, regexp_error);
    }
    else
    {
      if (ovec0 == 0 && ovec1 == 0)
      {
        fclose(buf);
        free(ptr);
        eth_throw(args, eth_invalid_argument());
      }
      else
      {
        fwrite(p, 1, ovec0, buf);
        // --
        eth_t x = eth_create_string2(p + ovec0, ovec1 - ovec0);
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
          eth_throw(args, eth_type_error());
        }
        fwrite(eth_str_cstr(r), 1, eth_str_len(r), buf);
        eth_drop(r);
        p += ovec1;
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
  eth_use_symbol(regexp_error)

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
      eth_throw(args, regexp_error);
    }
    else
    {
      if (ovec[0] == 0 && ovec[1] == 0)
      {
        eth_drop(acc);
        eth_throw(args, eth_invalid_argument());
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
  eth_use_variant(some)
  eth_use_symbol(regexp_error)
  eth_args args = eth_start(2);
  eth_t re = eth_arg2(args, eth_regexp_type);
  eth_t str = eth_arg2(args, eth_string_type);
  const char *p = eth_str_cstr(str);
  int n = eth_exec_regexp(re, p, eth_str_len(str), 0);
  if (n == 0)
    eth_throw(args, regexp_error);
  else if (n < 0)
    eth_return(args, eth_false);
  else
    eth_return(args,
        eth_tup2(eth_num(eth_ovector()[0]), eth_num(eth_ovector()[1])));
}

static eth_t
_to_number(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }

  char *nptr = eth_str_cstr(x);
  char *endptr;
  eth_number_t ret = eth_strtonum(nptr, &endptr);

  if (endptr == nptr)
  {
    eth_drop(x);
    return eth_exn(eth_failure());
  }

  while (*endptr)
  {
    if (not isspace(*endptr))
    {
      eth_drop(x);
      return eth_exn(eth_failure());
    }
    endptr ++;
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
    return eth_exn(eth_type_error());
  }

  eth_t ret = eth_sym(eth_str_cstr(x));
  eth_drop(x);
  return ret;
}


int
ether_module(eth_module *mod, eth_root *topenv)
{
  eth_module *detail = eth_create_module("string.detail", NULL);
  eth_copy_module_path(eth_get_root_env(topenv), eth_get_env(detail));

  eth_define(detail, "__malloc", eth_create_proc(_malloc, 1, NULL, NULL));
  eth_define(detail, "__calloc", eth_create_proc(_calloc, 1, NULL, NULL));
  eth_define(detail, "__make", eth_create_proc(_make, 2, NULL, NULL));

  eth_define(detail, "__len", eth_create_proc(_strlen, 1, NULL, NULL));
  eth_define(detail, "__to_upper", eth_create_proc(_to_upper, 1, NULL, NULL));
  eth_define(detail, "__to_lower", eth_create_proc(_to_lower, 1, NULL, NULL));
  eth_define(detail, "__strcmp", eth_create_proc(_strcmp, 2, NULL, NULL));
  eth_define(detail, "__strcasecmp", eth_create_proc(_strcasecmp, 2, NULL, NULL));
  eth_define(detail, "__substr", eth_create_proc(_substr, 3, NULL, NULL));
  eth_define(detail, "__strstr_opt", eth_create_proc(_strstr_opt, 2, NULL, NULL));
  eth_define(detail, "__cat", eth_create_proc(_cat, 1, NULL, NULL));
  eth_define(detail, "__chomp", eth_create_proc(_chomp, 1, NULL, NULL));
  eth_define(detail, "__chop", eth_create_proc(_chop, 1, NULL, NULL));
  eth_define(detail, "__trim_left", eth_create_proc(_trim_left, 1, NULL, NULL));
  eth_define(detail, "__trim_right", eth_create_proc(_trim_right, 1, NULL, NULL));
  eth_define(detail, "__chr", eth_create_proc(_chr, 1, NULL, NULL));
  eth_define(detail, "__ord", eth_create_proc(_ord, 1, NULL, NULL));
  // --
  eth_define(detail,  "__to_number", eth_create_proc( _to_number, 1, NULL, NULL));
  eth_define(detail,  "__to_symbol", eth_create_proc( _to_symbol, 1, NULL, NULL));
  // --
  eth_define(detail, "__match", eth_create_proc(_match, 2, NULL, NULL));
  eth_define(detail, "__gsub", eth_create_proc(_gsub, 3, NULL, NULL));
  eth_define(detail, "__rev_split", eth_create_proc(_rev_split, 2, NULL, NULL));
  eth_define(detail, "__find_regexp", eth_create_proc(_find_regexp, 2, NULL, NULL));

  eth_module *aux = eth_load_module_from_script2(topenv, "./lib.eth", NULL, detail);
  eth_destroy_module(detail);
  if (not aux)
    return -1;

  eth_copy_defs(aux, mod);
  eth_destroy_module(aux);

  return 0;
}

