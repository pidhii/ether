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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

eth_type *eth_string_type;

static eth_t g_chars[256];

static void
destroy_string(eth_type *type, eth_t x)
{
  free(ETH_STRING(x)->cstr);
  free(x);
}

static void
write_string(eth_type *type, eth_t x, FILE *out)
{
  putc('"', out);
  const char *p = eth_str_cstr(x);
  for (int i = 0; i < eth_str_len(x); ++i)
  {
    if (isprint(p[i]))
      putc(p[i], out);
    else
    {
      switch (p[i])
      {
        case '\0': fputs("\\0", out); break;
        case '\a': fputs("\\a", out); break;
        case '\b': fputs("\\b", out); break;
        case '\f': fputs("\\f", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        case '\v': fputs("\\v", out); break;
        default: fprintf(out, "\\x%hhx", p[i]);
      }
    }
  }
  putc('"', out);
}

static void
display_string(eth_type *type, eth_t x, FILE *out)
{
  fwrite(eth_str_cstr(x), 1, eth_str_len(x), out);
}

static bool
string_equal(eth_type *type, eth_t x, eth_t y)
{
  return eth_str_len(x) == eth_str_len(y)
     and memcmp(eth_str_cstr(x), eth_str_cstr(y), eth_str_len(x)) == 0;
}

void
_eth_init_strings(void)
{
  eth_string_type = eth_create_type("string");
  eth_string_type->destroy = destroy_string;
  eth_string_type->write = write_string;
  eth_string_type->display = display_string;
  eth_string_type->equal = string_equal;

  for (int i = 0; i < 256; ++i)
  {
    char *s = malloc(2);
    s[0] = i;
    s[1] = '\0';
    g_chars[i] = eth_create_string_from_ptr2(s, 1);
    eth_ref(g_chars[i]);
  }
}

void
_eth_cleanup_strings(void)
{
  for (int i = 0; i < 256; ++i)
    eth_unref(g_chars[i]);
  eth_destroy_type(eth_string_type);
}

eth_t
eth_create_string_from_ptr2(char *cstr, int len)
{
  eth_string *str = malloc(sizeof(eth_string));
  eth_init_header(str, eth_string_type);
  str->len = len;
  str->cstr = cstr;
  return ETH(str);
}

eth_t
eth_create_string_from_ptr(char *cstr)
{
  return eth_create_string_from_ptr2(cstr, strlen(cstr));
}

eth_t
eth_create_string(const char *cstr)
{
  int len = strlen(cstr);
  char *mystr = malloc(len + 1);
  memcpy(mystr, cstr, len + 1);
  return eth_create_string_from_ptr2(mystr, len);
}

eth_t
eth_create_string2(const char *str, int len)
{
  char *mystr = malloc(len + 1);
  memcpy(mystr, str, len);
  mystr[len] = 0;
  return eth_create_string_from_ptr2(mystr, len);
}

eth_t
eth_create_string_from_char(char c)
{
  return g_chars[(uint8_t)c];
}

