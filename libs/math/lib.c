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

#include <math.h>

#if ETH_NUMBER_TYPE == ETH_NUMBER_LONGDOUBLE
#define GNAME(name) name##l
#elif ETH_NUMBER_TYPE == ETH_NUMBER_DOUBLE
#define GNAME(name) name
#elif ETH_NUMBER_TYPE == ETH_NUMBER_FLOAT
#define GNAME(name) name##f
#else
#error undefine ETH_NUMBER_TYPE type
#endif

#define UNARY(name, func)                      \
  static eth_t                                 \
  name(void)                                   \
  {                                            \
    eth_t x = *eth_sp++;                       \
    if (eth_unlikely(not eth_is_num(x)))       \
    {                                          \
      eth_drop(x);                             \
      return eth_exn(eth_type_error());        \
    }                                          \
    eth_t ret = eth_num(func(eth_num_val(x))); \
    eth_drop(x);                               \
    return ret;                                \
  }

#define BINARY(name, func)                                     \
  static eth_t                                                 \
  name(void)                                                   \
  {                                                            \
    eth_t x = *eth_sp++;                                       \
    eth_t y = *eth_sp++;                                       \
    if (eth_unlikely(not eth_is_num(x) or not eth_is_num(y)))  \
    {                                                          \
      eth_drop_2(x, y);                                        \
      return eth_exn(eth_type_error());                        \
    }                                                          \
    eth_t ret = eth_num(func(eth_num_val(x), eth_num_val(y))); \
    eth_drop_2(x, y);                                          \
    return ret;                                                \
  }

#define TERNARY(name, func)                                                        \
  static eth_t                                                                     \
  name(void)                                                                       \
  {                                                                                \
    eth_t x = *eth_sp++;                                                           \
    eth_t y = *eth_sp++;                                                           \
    eth_t z = *eth_sp++;                                                           \
    if (eth_unlikely(not eth_is_num(x) or not eth_is_num(y) or not eth_is_num(z))) \
    {                                                                              \
      eth_drop_3(x, y, z);                                                         \
      return eth_exn(eth_type_error());                                            \
    }                                                                              \
    eth_t ret = eth_num(func(eth_num_val(x), eth_num_val(y), eth_num_val(z)));     \
    eth_drop_3(x, y, z);                                                           \
    return ret;                                                                    \
  }

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
// Trigonometric functions
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// - cos
//
UNARY(_cos, GNAME(cos))

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
// Hyperbolic functions
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// - cosh
//
UNARY(_cosh, GNAME(cosh))

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
// Invers trigonometric functions
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// - asin
// - acos
// - atan2
// - atan
//
UNARY(_asin, GNAME(asin))
UNARY(_acos, GNAME(acos))
BINARY(_atan2, GNAME(atan2))
UNARY(_atan, GNAME(atan))

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
// Invers hyperbolic functions
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// - asinh
// - acosh
// - atanh
//
UNARY(_asinh, GNAME(asinh))
UNARY(_acosh, GNAME(acosh))
UNARY(_atanh, GNAME(atanh))

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
// Arithmetics
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// - cbrt
// - exp2
// - exp
// - expm1
// - abs
// - fdim
// - fma
// - min
// - max
// - minmax
// - erfc
// - erf
// - hypot
//
UNARY(_sqrt, GNAME(sqrt))
UNARY(_cbrt, GNAME(cbrt))
UNARY(_exp2, GNAME(exp2))
UNARY(_exp, GNAME(exp))
UNARY(_expm1, GNAME(expm1))
UNARY(_abs, GNAME(fabs))
BINARY(_fdim, GNAME(fdim))
TERNARY(_fma, GNAME(fma))
BINARY(_max, GNAME(fmax))
BINARY(_min, GNAME(fmin))
static eth_t
_minmax(void)
{
  eth_t x = *eth_sp++;
  eth_t y = *eth_sp++;
  if (eth_unlikely(not eth_is_num(x) or not eth_is_num(y)))
  {
    eth_drop_2(x, y);
    return eth_exn(eth_type_error());
  }
  if (eth_num_val(x) < eth_num_val(y))
    return eth_create_tuple_2(x, y);
  else
    return eth_create_tuple_2(y, x);
}
UNARY(_erfc, GNAME(erfc))
UNARY(_erf, GNAME(erf))
BINARY(_hypot, GNAME(hypot))

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
// Rounding
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// - ceil
// - floor
//
UNARY(_ceil, GNAME(ceil))
UNARY(_floor, GNAME(floor))

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
// Miscelenious
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// - copysign
// - frexp
//
BINARY(_copysign, GNAME(copysign))
static eth_t
_frexp(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_num(x)))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  int exp;
  eth_t integ = eth_num(GNAME(frexp)(eth_num_val(x), &exp));
  eth_drop(x);
  return eth_create_tuple_2(integ, eth_num(exp));
}

UNARY(_log, GNAME(log))

static eth_t
_nanp(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_num(x)))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  eth_number_t xval = eth_num_val(x);
  eth_drop(x);
  return eth_boolean(isnan(xval));
}

int
ether_module(eth_module *mod)
{
  eth_define(mod, "cos", eth_create_proc(_cos, 1, NULL, NULL));
  // --
  eth_define(mod, "cosh", eth_create_proc(_cosh, 1, NULL, NULL));
  // --
  eth_define(mod, "asin", eth_create_proc(_asin, 1, NULL, NULL));
  eth_define(mod, "acos", eth_create_proc(_acos, 1, NULL, NULL));
  eth_define(mod, "atan2", eth_create_proc(_atan2, 2, NULL, NULL));
  eth_define(mod, "atan", eth_create_proc(_atan, 1, NULL, NULL));
  // --
  eth_define(mod, "asinh", eth_create_proc(_asin, 1, NULL, NULL));
  eth_define(mod, "acosh", eth_create_proc(_acos, 1, NULL, NULL));
  eth_define(mod, "atanh", eth_create_proc(_atanh, 1, NULL, NULL));
  // --
  eth_define(mod, "sqrt", eth_create_proc(_sqrt, 1, NULL, NULL));
  eth_define(mod, "cbrt", eth_create_proc(_cbrt, 1, NULL, NULL));
  eth_define(mod, "exp2", eth_create_proc(_exp2, 1, NULL, NULL));
  eth_define(mod, "exp", eth_create_proc(_exp, 1, NULL, NULL));
  eth_define(mod, "expm1", eth_create_proc(_expm1, 1, NULL, NULL));
  eth_define(mod, "abs", eth_create_proc(_abs, 1, NULL, NULL));
  eth_define(mod, "fdim", eth_create_proc(_fdim, 2, NULL, NULL));
  eth_define(mod, "fma", eth_create_proc(_fma, 3, NULL, NULL));
  eth_define(mod, "max", eth_create_proc(_max, 2, NULL, NULL));
  eth_define(mod, "min", eth_create_proc(_min, 2, NULL, NULL));
  eth_define(mod, "minmax", eth_create_proc(_minmax, 2, NULL, NULL));
  eth_define(mod, "erfc", eth_create_proc(_erfc, 1, NULL, NULL));
  eth_define(mod, "erf", eth_create_proc(_erf, 1, NULL, NULL));
  eth_define(mod, "hypot", eth_create_proc(_hypot, 2, NULL, NULL));
  // --
  eth_define(mod, "ceil", eth_create_proc(_ceil, 1, NULL, NULL));
  eth_define(mod, "floor", eth_create_proc(_floor, 1, NULL, NULL));
  // --
  eth_define(mod, "copysing", eth_create_proc(_copysign, 2, NULL, NULL));
  eth_define(mod, "frexp", eth_create_proc(_frexp, 1, NULL, NULL));

  eth_define(mod, "log", eth_create_proc(_log, 1, NULL, NULL));

  eth_define(mod, "nan?", eth_create_proc(_nanp, 1, NULL, NULL));

  return 0;
}

