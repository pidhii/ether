#include "ether/ether.h"

#include <stdlib.h>

static void
set_pub(eth_ast_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
    case ETH_PATTERN_CONSTANT:
      break;

    case ETH_PATTERN_IDENT:
      if (not pat->ident.attr)
        eth_ref_attr(pat->ident.attr = eth_create_attr(0));
      pat->ident.attr->flag |= ETH_ATTR_PUB;
      break;

    case ETH_PATTERN_UNPACK:
      for (int i = 0; i < pat->unpack.n; ++i)
        set_pub(pat->unpack.subpats[i]);
      break;

    case ETH_PATTERN_RECORD:
      for (int i = 0; i < pat->record.n; ++i)
        set_pub(pat->record.subpats[i]);
      break;
  }
}

eth_t
eth_eval(eth_env *topenv, eth_module *mod, eth_ast *ast)
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
      eth_ir *ir = eth_build_module_ir(ast, topenv, mod, &defs, mod);
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
        eth_define_attr(mod, defs.idents[i], eth_car(it), defs.attrs[i]);
      eth_destroy_ir_defs(&defs);
      eth_unref(ret);
      return eth_nil;
    }

    default:
    {
      eth_ir *ir = eth_build_ir(ast, topenv, mod);
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


