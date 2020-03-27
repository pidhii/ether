#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

eth_type *eth_boolean_type;
eth_t eth_true, eth_false;
eth_t eth_false_true[2];

static void
write_boolean(eth_type *type, eth_t x, FILE *stream)
{
  fputs(x == eth_true ? "true" : "false", stream);
}

void
_eth_init_boolean_type(void)
{
  static eth_header g_true, g_false;
  
  eth_boolean_type = eth_create_type("boolean");
  eth_boolean_type->write = write_boolean;
  eth_boolean_type->display = write_boolean;

  eth_true = &g_true;
  eth_init_header(eth_true, eth_boolean_type);
  eth_ref(eth_true);

  eth_false = &g_false;
  eth_init_header(eth_false, eth_boolean_type);
  eth_ref(eth_false);

  eth_false_true[0] = eth_false;
  eth_false_true[1] = eth_true;
}
