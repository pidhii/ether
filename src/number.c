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
#include <math.h>

eth_type *eth_number_type;

static void
number_destroy(eth_type *type, eth_t num)
{
  eth_free_h2(num);
}

static void
number_write(eth_type *type, eth_t x, FILE *out)
{
  eth_number_t val = ETH_NUMBER(x)->val;
  if ((val == INFINITY) | (val == -INFINITY))
  {
    fputs(val < 0 ? "-inf" : "inf", out);
    return;
  }

#if ETH_NUMBER_TYPE == ETH_NUMBER_LONGDOUBLE
  long double i, f = modfl(val, &i);
  fprintf(out, f == 0 ? "%.Lf" : "%Lg", val);
#elif ETH_NUMBER_TYPE == ETH_NUMBER_DOUBLE
  double i, f = modf(val, &i);
  fprintf(out, f == 0 ? "%.f" : "%g", val);
#else
  float i, f = modff(val, &i);
  fprintf(out, f == 0 ? "%.f" : "%g", val);
#endif
}

static void
number_display(eth_type *type, eth_t x, FILE *out)
{
  eth_number_t val = ETH_NUMBER(x)->val;
  if ((val == INFINITY) | (val == -INFINITY))
  {
    fputs(val < 0 ? "-inf" : "inf", out);
    return;
  }
#if ETH_NUMBER_TYPE == ETH_NUMBER_LONGDOUBLE
  long double i, f = modfl(val, &i);
  fprintf(out, f == 0 ? "%.Lf" : "%Lg", val);
#elif ETH_NUMBER_TYPE == ETH_NUMBER_DOUBLE
  double i, f = modf(val, &i);
  fprintf(out, f == 0 ? "%.f" : "%g", val);
#else
  float i, f = modff(val, &i);
  fprintf(out, f == 0 ? "%.f" : "%g", val);
#endif
}

static bool
number_equal(eth_type *type, eth_t x, eth_t y)
{
  return eth_num_val(x) == eth_num_val(y);
}

void
_eth_init_number_type(void)
{
  eth_number_type = eth_create_type("number");
  eth_number_type->destroy = number_destroy;
  eth_number_type->write = number_write;
  eth_number_type->display = number_display;
  eth_number_type->equal = number_equal;
}

