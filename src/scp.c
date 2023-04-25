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

ETH_MODULE("scp")

eth_scp*
eth_create_scp(eth_function **fns, int nfns)
{
  eth_scp *scp = eth_malloc(sizeof(eth_scp));
  scp->fns = eth_malloc(sizeof(eth_function*) * nfns);
  scp->nfns = nfns;
  scp->rc = nfns;
  memcpy(scp->fns, fns, sizeof(eth_function*) * nfns);

  for (int i = 0; i < nfns; ++i)
    fns[i]->clos.scp = scp;

  return scp;
}

void
eth_destroy_scp(eth_scp *scp)
{
  free(scp->fns);
  free(scp);
}

void
eth_drop_out(eth_scp *scp)
{
  size_t nfns = scp->nfns;
  eth_function **restrict fns = scp->fns;

  if (--scp->rc != 0)
    return;

  // validate scope-RC:
  size_t rc = 0;
  for (size_t i = 0; i < nfns; ++i)
    rc += (fns[i]->header.rc != 0);
  if ((scp->rc = rc) != 0)
    return;

  // Drop scope:
  // 1. deactivate closures
  // 2. release closures
  for (size_t i = 0; i < nfns; ++i)
    eth_deactivate_clos(fns[i]);
  // --
  for (size_t i = 0; i < nfns; ++i)
    eth_delete(ETH(fns[i]));

  eth_destroy_scp(scp);
}
