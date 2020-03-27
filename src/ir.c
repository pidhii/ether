#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

ETH_MODULE("ether:ir")

eth_ir*
eth_create_ir(eth_ir_node *body, int nvars)
{
  eth_ir *ir = malloc(sizeof(eth_ir));
  ir->rc = 0;
  eth_ref_ir_node(ir->body = body);
  ir->nvars = nvars;
  return ir;
}

static void
destroy_ir(eth_ir *ir)
{
  eth_unref_ir_node(ir->body);
  free(ir);
}

void
eth_ref_ir(eth_ir *ir)
{
  ir->rc += 1;
}

void
eth_drop_ir(eth_ir *ir)
{
  if (ir->rc == 0)
    destroy_ir(ir);
}

void
eth_unref_ir(eth_ir *ir)
{
  if (--ir->rc == 0)
    destroy_ir(ir);
}

