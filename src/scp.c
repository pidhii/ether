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
eth_create_scp(eth_function **clos, int nclos)
{
  eth_scp *scp = malloc(sizeof(eth_scp));
  scp->clos = malloc(sizeof(eth_function*) * nclos);
  scp->nclos = nclos;
  scp->rc = nclos;
  memcpy(scp->clos, clos, sizeof(eth_function*) * nclos);

  for (int i = 0; i < nclos; ++i)
    clos[i]->clos.scp = scp;

  return scp;
}

void
eth_destroy_scp(eth_scp *scp)
{
  free(scp->clos);
  free(scp);
}

void
eth_drop_out(eth_scp *scp)
{
  size_t nclos = scp->nclos;
  eth_function **restrict clos = scp->clos;

  if (--scp->rc != 0)
    return;

  // validate scope-RC:
  size_t rc = 0;
  for (size_t i = 0; i < nclos; ++i)
    rc += (clos[i]->header.rc != 0);
  if ((scp->rc = rc) != 0)
    return;

  // Drop scope:
  // 1. deactivate closures
  // 2. release closures
  for (size_t i = 0; i < nclos; ++i)
    eth_deactivate_clos(clos[i]);
  // --
  for (size_t i = 0; i < nclos; ++i)
    eth_delete(ETH(clos[i]));

  eth_destroy_scp(scp);
}
