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

eth_type *eth_lazy_type;

static void
destroy_lazy(eth_type *__attribute((unused)) type, eth_t x)
{
  eth_lazy *lazy = (eth_lazy *)x;
  eth_unref(lazy->value);
  eth_free_h2(lazy);
}

static eth_t
apply_impl(void)
{
  eth_args args = eth_start(1);
  eth_t lazy = eth_arg2(args, eth_lazy_type);
  eth_return(args, eth_get_lazy_value(lazy));
}

ETH_TYPE_CONSTRUCTOR(init_lazy_type)
{
  eth_lazy_type = eth_create_type("lazy");
  eth_lazy_type->destroy = destroy_lazy;
  eth_add_method(eth_lazy_type->methods, eth_apply_method,
                 eth_proc(apply_impl, 1));
}
