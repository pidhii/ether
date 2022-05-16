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

eth_env*
eth_get_evaluator_env(eth_evaluator *evl)
{
  return eth_get_root_env(evl->root);
}

static void
set_pub(eth_ast_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_AST_PATTERN_DUMMY:
    case ETH_AST_PATTERN_CONSTANT:
      break;

    case ETH_AST_PATTERN_IDENT:
      if (not pat->ident.attr)
        eth_ref_attr(pat->ident.attr = eth_create_attr(0));
      pat->ident.attr->flag |= ETH_ATTR_PUB;
      break;

    case ETH_AST_PATTERN_UNPACK:
      for (int i = 0; i < pat->unpack.n; ++i)
        set_pub(pat->unpack.subpats[i]);
      break;

    case ETH_AST_PATTERN_RECORD:
      for (int i = 0; i < pat->record.n; ++i)
        set_pub(pat->record.subpats[i]);
      break;

    case ETH_AST_PATTERN_RECORD_STAR:
      if (not pat->recordstar.attr)
        eth_ref_attr(pat->recordstar.attr = eth_create_attr(0));
      pat->recordstar.attr->flag |= ETH_ATTR_PUB;
      break;
  }
}

eth_t
eth_eval(eth_evaluator *evl, eth_ast *ast)
{
  switch (ast->tag)
  {
    case ETH_AST_LET:
    case ETH_AST_LETREC:
    {
      // add pub-qualifier for all variables
      for (int i = 0; i < ast->let.n; ++i)
        set_pub(ast->let.pats[i]);

      eth_ir_defs defs;
      eth_ir *ir =
        eth_build_module_ir(ast, evl->root, evl->mod, &defs, evl->mod);
      if (not ir)
        return NULL;

      eth_ssa *ssa = eth_build_ssa(ir, &defs);
      eth_drop_ir(ir);
      if (not ssa)
      {
        eth_destroy_ir_defs(&defs);
        return NULL;
      }

      eth_bytecode *bc = eth_build_bytecode(ssa);
      eth_drop_ssa(ssa);
      if (not bc)
      {
        eth_destroy_ir_defs(&defs);
        return NULL;
      }

      eth_t ret = eth_vm(bc);
      eth_ref(ret);
      eth_drop_bytecode(bc);
      if (ret->type == eth_exception_type)
      {
        eth_destroy_ir_defs(&defs);
        eth_unref(ret);
        return NULL;
      }

      // get defs:
      int i = 0;
      for (eth_t it = eth_tup_get(ret, 1); it != eth_nil; it = eth_cdr(it), ++i)
        eth_define_attr(evl->mod, defs.defs[i].ident, eth_car(it), defs.defs[i].attr);
      eth_destroy_ir_defs(&defs);
      eth_unref(ret);

      return eth_nil;
    }

    default:
    {
      eth_ir *ir = eth_build_ir(ast, evl->root, evl->mod);
      if (not ir)
        return NULL;

      eth_ssa *ssa = eth_build_ssa(ir, NULL);
      eth_drop_ir(ir);
      if (not ssa)
        return NULL;

      eth_bytecode *bc = eth_build_bytecode(ssa);
      eth_drop_ssa(ssa);
      if (not bc)
        return NULL;

      eth_t ret = eth_vm(bc);
      eth_ref(ret);
      eth_drop_bytecode(bc);
      eth_dec(ret);
      return ret;
    }
  }
}

