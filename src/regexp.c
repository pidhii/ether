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
  pcre_extra *extra;
};

static void
destroy_regexp(eth_type *type, eth_t x)
{
  eth_regexp *regexp = (void*)x;
  if (regexp->extra)
    pcre_free_study(regexp->extra);
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
  const char *_eptr = NULL;
  int _eoffs = 0;
  if (eptr == NULL) eptr = &_eptr;
  if (eoffs == NULL) eoffs = &_eoffs;

  pcre *re = pcre_compile(pat, opts, eptr, eoffs, NULL);
  if (re == NULL)
  {
    eth_debug("PCRE-compile failed: %s", eptr);
    return NULL;
  }
  eth_regexp *regexp = malloc(sizeof(eth_regexp));
  eth_init_header(regexp, eth_regexp_type);
  regexp->re = re;
  regexp->extra = NULL;
  return ETH(regexp);
}

void
eth_study_regexp(eth_t x)
{
  eth_regexp *regexp = (void*)x;
  if (not regexp->extra)
  {
    int opt = PCRE_STUDY_JIT_COMPILE | PCRE_STUDY_EXTRA_NEEDED;
    const char *err;
    regexp->extra = pcre_study(regexp->re, opt, &err);
  }
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
  int n = pcre_exec(regexp->re, regexp->extra, str, len, 0, opts, g_ovector,
      ETH_OVECTOR_SIZE);

  return n;
}

