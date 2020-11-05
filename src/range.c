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

eth_type *eth_rangelr_type, *eth_rangel_type, *eth_ranger_type;

static void
destroy_lr(eth_type *type, eth_t x)
{
  eth_rangelr *rng = (void*)x;
  eth_unref(rng->l);
  eth_unref(rng->r);
  eth_free_h2(rng);
}

static void
destroy_l(eth_type *type, eth_t x)
{
  eth_rangel *rng = (void*)x;
  eth_unref(rng->l);
  eth_free_h1(rng);
}

static void
destroy_r(eth_type *type, eth_t x)
{
  eth_ranger *rng = (void*)x;
  eth_unref(rng->r);
  eth_free_h1(rng);
}

static bool
rangelr_equal(eth_type *type, eth_t x, eth_t y)
{
  eth_rangelr *rng1 = (void*)x;
  eth_rangelr *rng2 = (void*)y;
  return eth_equal(rng1->l, rng2->l) and eth_equal(rng1->r, rng2->r);
}

static bool
rangel_equal(eth_type *type, eth_t x, eth_t y)
{
  eth_rangel *rng1 = (void*)x;
  eth_rangel *rng2 = (void*)y;
  return eth_equal(rng1->l, rng2->l);
}

static bool
ranger_equal(eth_type *type, eth_t x, eth_t y)
{
  eth_ranger *rng1 = (void*)x;
  eth_ranger *rng2 = (void*)y;
  return eth_equal(rng1->r, rng2->r);
}

void
_eth_init_range_types(void)
{
  char *fieldslr[] = { "l", "r" };
  char *fieldsl[] = { "l" };
  char *fieldsr[] = { "r" };
  ptrdiff_t offslr[] = { offsetof(eth_rangelr, l), offsetof(eth_rangelr, r) };
  ptrdiff_t offsl[] = { offsetof(eth_rangel, l) };
  ptrdiff_t offsr[] = { offsetof(eth_ranger, r) };
  eth_rangelr_type = eth_create_struct_type("rangelr", fieldslr, offslr, 2);
  eth_rangel_type = eth_create_struct_type("rangel", fieldsl, offsl, 1);
  eth_ranger_type = eth_create_struct_type("ranger", fieldsr, offsr, 1);
  eth_rangelr_type->destroy = destroy_lr;
  eth_rangel_type->destroy = destroy_l;
  eth_ranger_type->destroy = destroy_r;
  eth_rangelr_type->equal = rangelr_equal;
  eth_rangel_type->equal = rangel_equal;
  eth_ranger_type->equal = ranger_equal;
  eth_rangelr_type->flag |= ETH_TFLAG_PLAIN;
  eth_rangel_type->flag |= ETH_TFLAG_PLAIN;
  eth_ranger_type->flag |= ETH_TFLAG_PLAIN;
}

