#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

eth_type *eth_string_type;

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
      fprintf(out, "%#hhx", *p);
  }
  putc('"', out);
}

static void
display_string(eth_type *type, eth_t x, FILE *out)
{
  fputs(ETH_STRING(x)->cstr, out);
}

void
_eth_init_string_type(void)
{
  eth_string_type = eth_create_type("string");
  eth_string_type->destroy = destroy_string;
  eth_string_type->write = write_string;
  eth_string_type->display = display_string;
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

