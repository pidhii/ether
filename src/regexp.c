#include "pcre.h"

#include "ether/ether.h"

#include <string.h>


ETH_MODULE("ether:regexp")


eth_type *eth_regexp_type;


#define ETH_OVECTOR_N 0x64
#define ETH_OVECTOR_SIZE (ETH_OVECTOR_N * 3)

static
int g_ovector[ETH_OVECTOR_SIZE];

const int*
eth_ovector(void)
{
  return g_ovector;
}

struct eth_regexp {
  eth_header header;
  pcre *re;
};

static void
destroy_regexp(eth_type *type, eth_t x)
{
  eth_regexp *regexp = (void*)x;
  pcre_free(regexp->re);
  free(regexp);
}

void
_eth_init_regexp_type(void)
{
  eth_regexp_type = eth_create_type("regexp");
  eth_regexp_type->destroy = destroy_regexp;
}

eth_t
eth_create_regexp(const char *pat, int opts, const char **eptr, int *eoffs)
{
  const char *_eptr;
  int _eoffs;
  if (eptr == NULL) eptr = &_eptr;
  if (eoffs == NULL) eoffs = &_eoffs;

  eth_debug("compile regexp of \"%s\"", pat);
  pcre *re = pcre_compile(pat, opts, eptr, eoffs, NULL);
  if (re == NULL)
  {
    eth_debug("PCRE-compile failed: %s", eptr);
    return NULL;
  }
  eth_regexp *regexp = malloc(sizeof(eth_regexp));
  eth_init_header(regexp, eth_regexp_type);
  regexp->re = re;
  return ETH(regexp);
}

int
eth_get_regexp_ncaptures(eth_t x)
{
  eth_regexp *regexp = (void*)x;
  int n;
  if (pcre_fullinfo(regexp->re, NULL, PCRE_INFO_CAPTURECOUNT, &n))
    return -1;
  return n;
}

int
eth_exec_regexp(eth_t x, const char *str, int len, int opts)
{
  if (len <= 0)
    len = strlen(str);

  int ncap = eth_get_regexp_ncaptures(x);
  if (ncap + 1 > ETH_OVECTOR_N)
  {
    eth_warning("regular expression has too many captures");
    return -1;
  }

  eth_regexp *regexp = (void*)x;
  int n = pcre_exec(regexp->re, NULL, str, len, 0, opts, g_ovector,
      ETH_OVECTOR_SIZE);

  return n;
}

