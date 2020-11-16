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

eth_type *eth_pair_type;

static void
destroy_pair(eth_type *type, eth_t x)
{
  while (x->type == eth_pair_type) {
    eth_t tmp = eth_cdr(x);
    eth_unref(eth_car(x));
    eth_free_h2(x);
    x = tmp;

    if (eth_unlikely(eth_dec(x) > 0))
      return;
  }
  eth_drop(x);
}

static void
write_pair(eth_type *type, eth_t x, FILE *stream)
{
  if (eth_is_proper_list(x))
  {
    putc('[', stream);
    while (x->type == eth_pair_type) {
      eth_write(eth_car(x), stream);
      x = eth_cdr(x);
      if (x != eth_nil)
        putc(',', stream);
    }
    putc(']', stream);
  }
  else
  {
    while (x->type == eth_pair_type) {
      if (eth_car(x)->type == eth_pair_type)
        putc('(', stream);
      eth_write(eth_car(x), stream);
      if (eth_car(x)->type == eth_pair_type)
        putc(')', stream);
      fputs("::", stream);
      x = eth_cdr(x);
    }
    eth_write(x, stream);
  }
}

static bool
pair_equal(eth_type *type, eth_t x, eth_t y)
{
  while (eth_is_pair(x) & eth_is_pair(y))
  {
    if (not eth_equal(eth_car(x), eth_car(y)))
      return false;
    x = eth_cdr(x);
    y = eth_cdr(y);
  }
  return eth_equal(x, y);
}

void
_eth_init_pair_type(void)
{
  char *fields[] = { "car", "cdr" };
  ptrdiff_t offs[] = { offsetof(eth_pair, car), offsetof(eth_pair, cdr) };
  eth_pair_type = eth_create_struct_type("pair", fields, offs, 2);
  eth_pair_type->destroy = destroy_pair;
  eth_pair_type->write = write_pair;
  eth_pair_type->equal = pair_equal;
}

