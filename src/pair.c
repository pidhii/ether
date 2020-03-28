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

    if (eth_unlikely(x->magic != ETH_MAGIC_NONE))
    {
      eth_unref(x);
      return;
    }

    if (eth_unlikely(eth_dec(x) > 0))
      return;
  }
  eth_drop(x);
}

static void
write_pair(eth_type *type, eth_t x, FILE *stream)
{
  while (x->type == eth_pair_type) {
    if (eth_car(x)->type == eth_pair_type)
      putc('(', stream);
    eth_write(eth_car(x), stream);
    if (eth_car(x)->type == eth_pair_type)
      putc(')', stream);
    putc(':', stream);
    x = eth_cdr(x);
  }
  eth_write(x, stream);
}

static void
display_pair(eth_type *type, eth_t x, FILE *stream)
{
  while (x->type == eth_pair_type) {
    if (eth_car(x)->type == eth_pair_type)
      putc('(', stream);
    eth_display(eth_car(x), stream);
    if (eth_car(x)->type == eth_pair_type)
      putc(')', stream);
    putc(':', stream);
    x = eth_cdr(x);
  }
  eth_display(x, stream);
}

void
_eth_init_pair_type(void)
{
  char *fields[] = { "car", "cdr" };
  ptrdiff_t offs[] = { offsetof(eth_pair, car), offsetof(eth_pair, cdr) };
  eth_pair_type = eth_create_struct_type("pair", fields, offs, 2);
  eth_pair_type->destroy = destroy_pair;
  eth_pair_type->write = write_pair;
  eth_pair_type->display = display_pair;
}

