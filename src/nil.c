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

eth_type *eth_nil_type;
eth_t eth_nil;

static void
write_nil(eth_type *__attribute((unused)) type, eth_t __attribute((unused)) x,
          FILE *out)
{
  fputs("nil", out);
}

static eth_t
len_impl(void)
{
  eth_drop(*eth_sp++);
  return eth_num(0);
}

ETH_TYPE_CONSTRUCTOR(init_nil_type)
{
  static eth_header nil;

  eth_nil_type = eth_create_type("nil");
  eth_nil_type->write = write_nil;
  eth_nil_type->display = write_nil;
  eth_add_method(eth_nil_type->methods, eth_len_method, eth_proc(len_impl, 1));

  eth_nil = &nil;
  eth_init_header(eth_nil, eth_nil_type);
  eth_ref(eth_nil);
}
