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
  for (char *p = ETH_STRING(x)->cstr; *p; ++p)
  {
    if (isprint((int)*p))
      putc(*p, out);
    else
    {
      switch (*p)
      {
        case '\0': fputs("\\0", out); break;
        case '\a': fputs("\\a", out); break;
        case '\b': fputs("\\b", out); break;
        case '\f': fputs("\\f", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        case '\v': fputs("\\v", out); break;
        default: fprintf(out, "\\%#hhx", *p);
      }
    }
  }
  putc('"', out);
}

static void
display_string(eth_type *type, eth_t x, FILE *out)
{
  fputs(ETH_STRING(x)->cstr, out);
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

