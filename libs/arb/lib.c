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

BINOP(_add, arb_add)
BINOP(_sub, arb_sub)
BINOP(_mul, arb_mul)
BINOP(_div, arb_div)
BINOP(_pow, arb_pow)

int
ether_module(eth_module *mod, eth_env *topenv)
{
  init_arb_type();
  eth_add_destructor(mod, eth_arb_type, (void*)eth_destroy_type);

  eth_define(mod, "+", eth_create_proc(_add, 2, NULL, NULL));
  eth_define(mod, "-", eth_create_proc(_sub, 2, NULL, NULL));
  eth_define(mod, "*", eth_create_proc(_mul, 2, NULL, NULL));
  eth_define(mod, "/", eth_create_proc(_div, 2, NULL, NULL));
  eth_define(mod, "^", eth_create_proc(_pow, 2, NULL, NULL));

  return 0;
}
