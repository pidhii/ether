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

eth_type* eth_exception_type;

static void
destroy_exception(eth_type *type, eth_t x)
{
  eth_exception *e = ETH_EXCEPTION(x);
  eth_unref(e->what);
  for (int i = 0; i < e->tracelen; ++i)
    eth_unref_location(e->trace[i]);
  free(e->trace);
  free(x);
}

static void
write_exception(eth_type *type, eth_t x, FILE *out)
{
  eth_fprintf(out, "exception { ~w }", ETH_EXCEPTION(x)->what);
}

void
_eth_init_exception_type(void)
{
  char *what = "what";
  ptrdiff_t offs = offsetof(eth_exception, what);
  eth_exception_type = eth_create_struct_type("exception", &what, &offs, 1);
  eth_exception_type->destroy = destroy_exception;
  eth_exception_type->write = write_exception;
}

eth_t
eth_create_exception(eth_t what)
{
  eth_exception *exn = malloc(sizeof(eth_exception));
  eth_init_header(exn, eth_exception_type);
  eth_ref(exn->what = what);
  exn->trace = malloc(0);
  exn->tracelen = 0;
  return ETH(exn);
}

eth_t
eth_copy_exception(eth_t exn)
{
  eth_exception *e = ETH_EXCEPTION(exn);
  eth_exception *ret = ETH_EXCEPTION(eth_create_exception(e->what));
  ret->tracelen = e->tracelen;
  ret->trace = realloc(ret->trace, sizeof(eth_location) * e->tracelen);
  for (int i = 0; i < e->tracelen; ++i)
    eth_ref_location(ret->trace[i] = e->trace[i]);
  return ETH(ret);
}

void
eth_push_trace(eth_t exn, eth_location *loc)
{
  eth_exception *e = ETH_EXCEPTION(exn);
  e->trace = realloc(e->trace, sizeof(eth_location) * (e->tracelen + 1));
  assert(e->trace);
  e->trace[e->tracelen++] = loc;
  eth_ref_location(loc);
}

