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

static eth_t
_strong(void)
{
  eth_t x = *eth_sp++;
  eth_t ref = eth_create_strong_ref(x);
  return ref;
}

static eth_t
_get(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_strong_ref_type))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  eth_t ret = eth_ref_get(x);
  eth_ref(ret);
  eth_drop(x);
  eth_dec(ret);
  return ret;
}

static eth_t
_set(void)
{
  eth_args args = eth_start(2);
  eth_t ref = eth_arg(args);
  eth_t x = eth_arg(args);
  if (ref->type == eth_strong_ref_type)
    eth_set_strong_ref(ref, x);
  else
    eth_throw(args, eth_exn(eth_type_error()));
  eth_return(args, eth_nil);
}

int
ether_module(eth_module *mod)
{
  eth_define(mod, "strong", eth_create_proc(_strong, 1, NULL, NULL));
  eth_define(mod, "get", eth_create_proc(_get, 1, NULL, NULL));
  eth_define(mod, "set", eth_create_proc(_set, 2, NULL, NULL));

  return 0;
}
