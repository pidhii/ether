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

#include <assert.h>
#include <stdlib.h>

eth_type *eth_exit_type;

static void
destroy_exit_object(eth_type *__attribute((unused)) type, eth_t x)
{
  free(x);
}

ETH_TYPE_CONSTRUCTOR(init_exit_type)
{
  // use struct-type to enable pattern-matching
  eth_exit_type = eth_create_struct_type("exit-object", NULL, 0);
  eth_exit_type->destroy = destroy_exit_object;
}

eth_t
eth_create_exit_object(int status)
{
  eth_exit_object *e = eth_malloc(sizeof(eth_exit_object));
  eth_init_header(e, eth_exit_type);
  e->status = status;
  return ETH(e);
}
