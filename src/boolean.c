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

eth_type *eth_boolean_type;
eth_t eth_true, eth_false;
eth_t eth_false_true[2];

static void
write_boolean(eth_type *type, eth_t x, FILE *stream)
{
  fputs(x == eth_true ? "true" : "false", stream);
}

void
_eth_init_boolean_type(void)
{
  static eth_header g_true, g_false;
  
  eth_boolean_type = eth_create_type("boolean");
  eth_boolean_type->write = write_boolean;
  eth_boolean_type->display = write_boolean;

  eth_true = &g_true;
  eth_init_header(eth_true, eth_boolean_type);
  eth_ref(eth_true);

  eth_false = &g_false;
  eth_init_header(eth_false, eth_boolean_type);
  eth_ref(eth_false);

  eth_false_true[0] = eth_false;
  eth_false_true[1] = eth_true;
}
