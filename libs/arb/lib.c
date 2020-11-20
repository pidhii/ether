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
#include <ether/ether.h>

#include <arb.h>

static
ulong eth_arb_precision = 0x1000;

typedef struct {
  eth_header header;
  arb_t val;
} eth_arb;
#define ARB(x) ((eth_arb*)(x))->val

static
eth_type *eth_arb_type;

static void
destroy_arb(eth_type *type, eth_t x)
{
  arb_clear(ARB(x));
  free(x);
}

static void
write_arb(eth_type *type, eth_t x, FILE *out)
{
  arb_fprintn(out, ARB(x), 5, 0);
}

static bool
equal_arb(eth_type *type, eth_t x, eth_t y)
{
  return arb_eq(ARB(x), ARB(y));
}

static void
init_arb_type(void)
{
  eth_arb_type = eth_create_type("arb");
  eth_arb_type->destroy = destroy_arb;
  eth_arb_type->write = write_arb;
  eth_arb_type->equal = equal_arb;
}

static eth_t
create_arb()
{
  eth_arb *arb = malloc(sizeof(eth_arb));
  eth_init_header(arb, eth_arb_type);
  arb_init(ARB(arb));
  return ETH(arb);
}

static eth_t
_get_precision(void)
{
  return eth_num(eth_arb_precision);
}

static eth_t
_set_precision(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_num(x)))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  if (eth_unlikely(not eth_is_unsigned_long(x)))
  {
    eth_drop(x);
    return eth_exn(eth_invalid_argument());
  }
  eth_arb_precision = eth_num_val(x);
  return eth_nil;
}

#define BINOP(name, arbproc)                                \
  static eth_t                                              \
  name(void)                                                \
  {                                                         \
    eth_t x = *eth_sp++;                                    \
    eth_t y = *eth_sp++;                                    \
                                                            \
    eth_t z = create_arb();                                 \
    if (x->type == eth_number_type)                         \
    {                                                       \
      if (y->type == eth_number_type)                       \
      {                                                     \
        arb_t xarb, yarb;                                   \
        arb_init(xarb);                                     \
        arb_init(yarb);                                     \
        arb_set_d(xarb, eth_num_val(x));                    \
        arb_set_d(yarb, eth_num_val(y));                    \
        arbproc(ARB(z), xarb, yarb, eth_arb_precision);     \
      }                                                     \
      else if (y->type == eth_arb_type)                     \
      {                                                     \
        arb_t xarb;                                         \
        arb_init(xarb);                                     \
        arb_set_d(xarb, eth_num_val(x));                    \
        arbproc(ARB(z), xarb, ARB(y), eth_arb_precision);   \
        arb_clear(xarb);                                    \
      }                                                     \
      else                                                  \
      {                                                     \
        eth_drop(z);                                        \
        eth_drop_2(x, y);                                   \
        return eth_type_error();                            \
      }                                                     \
    }                                                       \
    else if (x->type == eth_arb_type)                       \
    {                                                       \
      if (y->type == eth_number_type)                       \
      {                                                     \
        arb_t yarb;                                         \
        arb_init(yarb);                                     \
        arb_set_d(yarb, eth_num_val(y));                    \
        arbproc(ARB(z), ARB(x), yarb, eth_arb_precision);   \
        arb_clear(yarb);                                    \
      }                                                     \
      else if (y->type == eth_arb_type)                     \
      {                                                     \
        arbproc(ARB(z), ARB(x), ARB(y), eth_arb_precision); \
      }                                                     \
      else                                                  \
      {                                                     \
        eth_drop(z);                                        \
        eth_drop_2(x, y);                                   \
        return eth_type_error();                            \
      }                                                     \
    }                                                       \
    eth_drop_2(x, y);                                       \
    return z;                                               \
  }

#define BINOP_CMP(name, arbproc, op)       \
  static eth_t                             \
  name(void)                               \
  {                                        \
    eth_t x = *eth_sp++;                   \
    eth_t y = *eth_sp++;                   \
                                           \
    bool ret = 0;                          \
    if (x->type == eth_number_type)        \
    {                                      \
      if (y->type == eth_number_type)      \
      {                                    \
        eth_number_t lhs = eth_num_val(x); \
        eth_number_t rhs = eth_num_val(y); \
        ret = op;                          \
      }                                    \
      else if (y->type == eth_arb_type)    \
      {                                    \
        arb_t xarb;                        \
        arb_init(xarb);                    \
        arb_set_d(xarb, eth_num_val(x));   \
        ret = arbproc(xarb, ARB(y));       \
      }                                    \
      else                                 \
      {                                    \
        eth_drop_2(x, y);                  \
        return eth_type_error();           \
      }                                    \
    }                                      \
    else if (x->type == eth_arb_type)      \
    {                                      \
      if (y->type == eth_number_type)      \
      {                                    \
        arb_t yarb;                        \
        arb_init(yarb);                    \
        arb_set_d(yarb, eth_num_val(y));   \
        ret = arbproc(ARB(x), yarb);       \
      }                                    \
      else if (y->type == eth_arb_type)    \
      {                                    \
        ret = arbproc(ARB(x), ARB(y));     \
      }                                    \
      else                                 \
      {                                    \
        eth_drop_2(x, y);                  \
        return eth_type_error();           \
      }                                    \
    }                                      \
    eth_drop_2(x, y);                      \
    return eth_boolean(ret);               \
  }


BINOP(_add, arb_add)
BINOP(_sub, arb_sub)
BINOP(_mul, arb_mul)
BINOP(_div, arb_div)
BINOP(_pow, arb_pow)

BINOP_CMP(_eq, arb_eq, lhs == rhs)
BINOP_CMP(_ne, arb_ne, lhs != rhs)
BINOP_CMP(_lt, arb_lt, lhs < rhs)
BINOP_CMP(_le, arb_le, lhs <= rhs)
BINOP_CMP(_gt, arb_gt, lhs > rhs)
BINOP_CMP(_ge, arb_ge, lhs >= rhs)

static eth_t
_to_number(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_arb_type))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  double mid = arf_get_d(arb_midref(ARB(x)), ARF_RND_NEAR);
  eth_drop(x);
  return eth_num(mid);
}

static eth_t
_of_string(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_str(x)))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  eth_t ret = create_arb();
  if (not arb_set_str(ARB(ret), eth_str_cstr(x), eth_arb_precision))
  {
    eth_drop(x);
    eth_drop(ret);
    return eth_exn(eth_failure());
  }
  eth_drop(x);
  return ret;
}

int
ether_module(eth_module *mod, eth_root *topenv)
{
  // Init
  init_arb_type();
  eth_add_destructor(mod, eth_arb_type, (void*)eth_destroy_type);

  // Configuration
  eth_define(mod, "get_precision", eth_create_proc(_get_precision, 0, NULL, NULL));
  eth_define(mod, "set_precision", eth_create_proc(_set_precision, 1, NULL, NULL));

  // Arithmetic operators
  eth_define(mod, "+", eth_create_proc(_add, 2, NULL, NULL));
  eth_define(mod, "-", eth_create_proc(_sub, 2, NULL, NULL));
  eth_define(mod, "*", eth_create_proc(_mul, 2, NULL, NULL));
  eth_define(mod, "/", eth_create_proc(_div, 2, NULL, NULL));
  eth_define(mod, "^", eth_create_proc(_pow, 2, NULL, NULL));
  // Comparison operators
  eth_define(mod, "==", eth_create_proc(_eq, 2, NULL, NULL));
  eth_define(mod, "/=", eth_create_proc(_ne, 2, NULL, NULL));
  eth_define(mod, "<", eth_create_proc(_lt, 2, NULL, NULL));
  eth_define(mod, "<=", eth_create_proc(_le, 2, NULL, NULL));
  eth_define(mod, ">", eth_create_proc(_gt, 2, NULL, NULL));
  eth_define(mod, ">=", eth_create_proc(_ge, 2, NULL, NULL));

  // Type conversion
  eth_define(mod, "to_number", eth_create_proc(_to_number, 1, NULL, NULL));
  eth_define(mod, "of_string", eth_create_proc(_of_string, 1, NULL, NULL));


  return 0;
}
