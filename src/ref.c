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

eth_type *eth_strong_ref_type;

static void
destroy_strong_ref(eth_type *type, eth_t x)
{
  eth_unref(eth_ref_get(x));
  eth_free_h1(x);
}

void
_eth_init_ref_type(void)
{
  eth_strong_ref_type = eth_create_type("strong-ref");
  eth_strong_ref_type->destroy = destroy_strong_ref;
}

eth_t
eth_create_strong_ref(eth_t init)
{
  eth_mut_ref *ref = eth_alloc_h1();
  eth_init_header(ref, eth_strong_ref_type);
  eth_ref(ref->val = init);
  return ETH(ref);
}

