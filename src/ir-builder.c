#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


ETH_MODULE("ether:ir-builder")


typedef struct ir_builder {
  struct ir_builder *parent;
  eth_var_list *vars;
  int nvars;
  cod_vec(eth_type*) types;
} ir_builder;

static ir_builder*
create_ir_builder(ir_builder *parent)
{
  ir_builder *bldr = malloc(sizeof(ir_builder));
  bldr->parent = parent;
  bldr->vars = eth_create_var_list();
  bldr->nvars = 0;
  cod_vec_init(bldr->types);

  cod_vec_push(bldr->types, eth_pair_type);

  return bldr;
}

static void
destroy_ir_builder(ir_builder *bldr)
{
  eth_destroy_var_list(bldr->vars);
  cod_vec_destroy(bldr->types);
  free(bldr);
}

static inline int
new_vid(ir_builder *bldr)
{
  return bldr->nvars++;
}

/*
 * Require variable in the scope. Variable will be captured if not found in
 * current scope.
 *
 * Note: Constants wont be captured. If constant variable is returned, it's
 * constant values must be used instead.
 */
static eth_var*
require_var(ir_builder *bldr, const char *ident, int *cnt)
{
  int offs;
  eth_var *var = eth_find_var(bldr->vars->head, ident, &offs);
  if (var)
  {
    if (cnt) *cnt = offs;
    return var;
  }
  else if (bldr->parent)
  {
    if ((var = require_var(bldr->parent, ident, &offs)))
    {
      // immediately return constants (don't perform capture)
      if (var->cval) return var;
      if (cnt) *cnt = bldr->vars->len;
      int vid = new_vid(bldr);
      return eth_append_var(bldr->vars, eth_copy_var_cfg(var, vid));
    }
  }
  return NULL;
}

static int
load_c_module(ir_builder *bldr, eth_module *mod)
{
  for (int i = 0; i < mod->ndefs; ++i)
  {
    eth_var_cfg var = eth_const_var(mod->defs[i].ident, mod->defs[i].val);
    eth_prepend_var(bldr->vars, var);
  }
  return mod->ndefs;
}

static eth_ir_pattern*
build_pattern(ir_builder *bldr, eth_ast_pattern *pat, int *e)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_IDENT:
    {
      int varid = new_vid(bldr);
      eth_prepend_var(bldr->vars, eth_dyn_var(pat->ident.str, varid));
      return eth_ir_ident_pattern(varid);
    }

    case ETH_PATTERN_UNPACK:
    {
      eth_type *type = NULL;
      for (size_t i = 0; i < bldr->types.len; ++i)
      {
        eth_type *ty = bldr->types.data[i];
        if (strcmp(ty->name, pat->unpack.type) == 0)
        {
          type = ty;
          break;
        }
      }
      if (type == NULL)
      {
        eth_error("no such type '%s'", pat->unpack.type);
        *e = 1;
        return eth_ir_ident_pattern(-1); // just some dummy
      }

      int n = pat->unpack.n;
      int offs[n];
      eth_ir_pattern *pats[n];
      for (int i = 0; i < n; ++i)
      {
        eth_field *fld = eth_get_field(type, pat->unpack.fields[i]);
        if (fld == NULL)
        {
          eth_error("no field '%s' of type '%s'", pat->unpack.fields[i],
              type->name);
          *e = 1;
          return eth_ir_ident_pattern(-1); // just some dummy
        }
        offs[i] = fld->offs;
        pats[i] = build_pattern(bldr, pat->unpack.subpats[i], e);
      }
      return eth_ir_unpack_pattern(type, offs, pats, n);
    }
  }

  assert(!"WTF");
}

static eth_ir_node*
build(ir_builder *bldr, eth_ast *ast, int *e)
{
  switch (ast->tag)
  {
    case ETH_AST_CVAL:
    {
      return eth_ir_cval(ast->cval.val);
    }

    case ETH_AST_IDENT:
    {
      eth_var *var = require_var(bldr, ast->ident.str, NULL);
      if (var == NULL)
      {
        eth_warning("undefined variable, '%s'", ast->ident.str);
        *e = 1;
        return eth_ir_error();
      }
      else if (var->cval)
        return eth_ir_cval(var->cval);
      else
        return eth_ir_var(var->vid);
    }

    case ETH_AST_APPLY:
    {
      eth_ir_node *fn = build(bldr, ast->apply.fn, e);
      eth_ir_node *args[ast->apply.nargs];
      for (int i = 0; i < ast->apply.nargs; ++i)
        args[i] = build(bldr, ast->apply.args[i], e);
      return eth_ir_apply(fn, args, ast->apply.nargs);
    }

    case ETH_AST_IF:
    {
      eth_ir_node *cond = build(bldr, ast->iff.cond, e);
      eth_ir_node *thenbr = build(bldr, ast->iff.then, e);
      eth_ir_node *elsebr = build(bldr, ast->iff.els, e);
      return eth_ir_if(cond, thenbr, elsebr);
    }

    case ETH_AST_SEQ:
    {
      eth_ir_node *e1 = build(bldr, ast->seq.e1, e);
      eth_ir_node *e2 = build(bldr, ast->seq.e2, e);
      return eth_ir_seq(e1, e2);
    }

    case ETH_AST_LET:
    {
      // build values
      eth_ir_node *vals[ast->let.n];
      for (int i = 0; i < ast->let.n; ++i)
        vals[i] = build(bldr, ast->let.vals[i], e);
      // declare variables
      int vids[ast->let.n];
      for (int i = 0; i < ast->let.n; ++i)
      {
        vids[i] = new_vid(bldr);
        eth_prepend_var(bldr->vars, eth_dyn_var(ast->let.nams[i], vids[i]));
      }
      // build body
      eth_ir_node *body = build(bldr, ast->let.body, e);
      // hide variables
      eth_pop_var(bldr->vars, ast->let.n);
      return eth_ir_let(vids, vals, ast->let.n, body);
    }

    case ETH_AST_LETREC:
    {
      // declare variables
      int vids[ast->letrec.n];
      for (int i = 0; i < ast->letrec.n; ++i)
      {
        vids[i] = new_vid(bldr);
        eth_prepend_var(bldr->vars, eth_dyn_var(ast->letrec.nams[i], vids[i]));
      }
      // build values
      eth_ir_node *vals[ast->letrec.n];
      for (int i = 0; i < ast->letrec.n; ++i)
        vals[i] = build(bldr, ast->letrec.vals[i], e);
      // build body
      eth_ir_node *body = build(bldr, ast->letrec.body, e);
      // hide variables
      eth_pop_var(bldr->vars, ast->letrec.n);
      return eth_ir_letrec(vids, vals, ast->letrec.n, body);
    }

    case ETH_AST_BINOP:
    {
      eth_ir_node *lhs = build(bldr, ast->binop.lhs, e);
      eth_ir_node *rhs = build(bldr, ast->binop.rhs, e);
      return eth_ir_binop(ast->binop.op, lhs, rhs);
    }

    case ETH_AST_FN:
    {
      ir_builder *fnbldr = create_ir_builder(bldr);

      // arguments:
      int nargs = ast->fn.arity;
      int args[nargs];
      for (int i = 0; i < nargs; ++i)
      {
        args[i] = new_vid(fnbldr);
        eth_prepend_var(fnbldr->vars, eth_dyn_var(ast->fn.args[i], args[i]));
      }

      // body:
      eth_ir_node *body_ = build(fnbldr, ast->fn.body, e);
      eth_ir *body = eth_create_ir(body_, fnbldr->nvars);

      // resolve captures:
      eth_pop_var(fnbldr->vars, nargs); // pop args so only captures are left
      int ncap = fnbldr->vars->len;
      int caps[ncap];
      int capvars[ncap];
      int i = 0;
      for (eth_var *cap = fnbldr->vars->head; i < ncap; cap = cap->next, ++i)
      {
        assert(cap->vid >= 0);
        capvars[i] = cap->vid;

        eth_var *var = eth_find_var(bldr->vars->head, cap->ident, NULL);
        assert(var && var->vid >= 0);
        caps[i] = var->vid;
      }

      destroy_ir_builder(fnbldr);
      return eth_ir_fn(ast->fn.arity, caps, capvars, ncap, body);
    }

    case ETH_AST_MATCH:
    {
      eth_ir_node *expr = build(bldr, ast->match.expr, e);
      int n1 = bldr->vars->len;
      eth_ir_pattern *pat = build_pattern(bldr, ast->match.pat, e);
      int n2 = bldr->vars->len;
      int nvars = n2 - n1;
      eth_ir_node *thenbr = build(bldr, ast->match.thenbr, e);
      eth_pop_var(bldr->vars, nvars);
      eth_ir_node *elsebr = build(bldr, ast->match.elsebr, e);
      return eth_ir_match(pat, expr, thenbr, elsebr);
    }
  }

  eth_error("undefined AST-node");
  *e = 1;
  return eth_ir_error();
}

eth_ir*
eth_build_ir(eth_ast *ast, eth_environment *env)
{
  eth_ir *ret;
  int e = 0;

  ir_builder *bldr = create_ir_builder(NULL);
  if (env)
  {
    for (int imod = 0; imod < env->nmods; ++imod)
      load_c_module(bldr, env->mods[imod]);
  }

  eth_ir_node *body = build(bldr, ast, &e);
  if (e)
  {
    eth_drop_ir_node(body);
    ret = NULL;
  }
  else
  {
    ret = eth_create_ir(body, bldr->nvars);
  }
  destroy_ir_builder(bldr);
  return ret;
}

