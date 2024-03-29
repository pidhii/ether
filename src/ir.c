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

ETH_MODULE("ether:ir")

eth_ir*
eth_create_ir(eth_ir_node *body, int nvars)
{
  eth_ir *ir = eth_malloc(sizeof(eth_ir));
  ir->rc = 0;
  eth_ref_ir_node(ir->body = body);
  ir->nvars = nvars;
  ir->nspecs = 0;
  ir->specs = NULL;
  return ir;
}

static void
destroy_ir(eth_ir *ir)
{
  eth_unref_ir_node(ir->body);
  if (ir->specs)
  {
    for (int i = 0; i < ir->nspecs; ++i)
      eth_destroy_spec(ir->specs[i]);
    free(ir->specs);
  }
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

void
eth_add_spec(eth_ir *ir, eth_spec *spec)
{
  eth_spec **newspecs =
    reallocarray(ir->specs, ir->nspecs+1, sizeof(eth_spec*));
  if (newspecs == NULL)
  {
    eth_error("allocation failure");
    abort();
  }
  newspecs[ir->nspecs] = spec;
  ir->specs = newspecs;
  ir->nspecs += 1;
}

