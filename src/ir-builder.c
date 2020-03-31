#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


ETH_MODULE("ether:ir-builder")


typedef struct ir_builder {
  struct ir_builder *parent;
  eth_env *env;
  bool istoplvl;
  eth_var_list *vars;
  int nvars;
  cod_vec(eth_type*) types;
  cod_vec(eth_module*) mods;
  cod_vec(char*) defidents;
  cod_vec(int  ) defvarids;
} ir_builder;

static ir_builder*
create_ir_builder(ir_builder *parent)
{
  ir_builder *bldr = malloc(sizeof(ir_builder));
  bldr->parent = parent;
  bldr->env = parent ? parent->env : NULL;
  bldr->istoplvl = false;
  bldr->vars = eth_create_var_list();
  bldr->nvars = 0;
  cod_vec_init(bldr->types);
  cod_vec_init(bldr->mods);
  cod_vec_init(bldr->defidents);
  cod_vec_init(bldr->defvarids);

  if (parent)
  {
    cod_vec_iter(parent->types, i, x, cod_vec_push(bldr->types, x));
    cod_vec_iter(parent->mods, i, x, cod_vec_push(bldr->mods, x));
  }
  else
  {
    cod_vec_push(bldr->types, eth_pair_type);
  }

  return bldr;
}

static void
destroy_ir_builder(ir_builder *bldr)
{
  eth_destroy_var_list(bldr->vars);
  cod_vec_destroy(bldr->types);
  cod_vec_destroy(bldr->mods);
  cod_vec_iter(bldr->defidents, i, x, free(x));
  cod_vec_destroy(bldr->defidents);
  cod_vec_destroy(bldr->defvarids);
  free(bldr);
}

static eth_ir_node*
build(ir_builder *bldr, eth_ast *ast, int *e);

static inline int
new_vid(ir_builder *bldr)
{
  return bldr->nvars++;
}

static inline void
trace_pub_var(ir_builder *bldr, const char *ident, int varid, int *e)
{
  if (bldr->istoplvl)
  {
    cod_vec_push(bldr->defidents, strdup(ident));
    cod_vec_push(bldr->defvarids, varid);
  }
  else
  {
    eth_warning("can't have public variables outside top level-scope");
    *e = 1;
  }
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

static bool
load_module(ir_builder *bldr, const eth_module *mod, const char *alias,
    char *const nams[], int nnam)
{
  const char *modname = alias ? alias : mod->name;
  char prefix[strlen(modname) + 2];
  if (modname[0])
    sprintf(prefix, "%s.", modname);
  else
    prefix[0] = 0;

  char buf[124];
  if (nams)
  {
    for (int i = 0; i < nnam; ++i)
    {
      eth_def *def = eth_find_def(mod, nams[i]);
      if (not def)
      {
        eth_warning("no '%s' in module %s", nams[i], mod->name);
        return false;
      }
      assert(strlen(def->ident) + sizeof prefix < sizeof buf);
      sprintf(buf, "%s%s", prefix, def->ident);
      eth_var_cfg var = eth_const_var(buf, def->val);
      eth_prepend_var(bldr->vars, var);
    }
    return true;
  }
  else
  {
    // import all identifiers
    for (int i = 0; i < mod->ndefs; ++i)
    {
      char *ident = mod->defs[i].ident;
      assert(strlen(ident) + sizeof prefix < sizeof buf);
      sprintf(buf, "%s%s", prefix, ident);
      eth_var_cfg var = eth_const_var(buf, mod->defs[i].val);
      eth_prepend_var(bldr->vars, var);
    }
    return true;
  }
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
      if (pat->ident.pub)
        trace_pub_var(bldr, pat->ident.str, varid, e);
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

  eth_error("wtf");
  abort();
}

static eth_ir_node*
build_with_toplvl(ir_builder *bldr, eth_ast *ast, int *e, bool savetop)
{
  bool tlvl = bldr->istoplvl;
  if (not savetop)
    bldr->istoplvl = false;
  eth_ir_node *ret = build(bldr, ast, e);
  bldr->istoplvl = tlvl;
  return ret;
}

// TODO: flag as likely
static eth_ir_node*
build_let(ir_builder *bldr, int idx, eth_ast *ast, eth_ir_node *const vals[],
    int nvars0, int *e)
{
  if (idx < ast->let.n)
  {
    eth_ir_pattern *pat = build_pattern(bldr, ast->let.pats[idx], e);

    eth_ir_node *thenbr = build_let(bldr, idx + 1, ast, vals, nvars0, e);

    eth_ir_node *raise = eth_ir_cval(eth_get_builtin("raise"));
    eth_ir_node *what = eth_ir_cval(eth_str("type-error"));
    eth_ir_node *elsebr = eth_ir_apply(raise, &what, 1);

    eth_ir_node *ret = eth_ir_match(pat, vals[idx], thenbr, elsebr);
    ret->match.toplvl = ETH_TOPLVL_THEN;
    return ret;
  }
  else
  {
    int nvars = bldr->nvars - nvars0;
    eth_ir_node *ret = build(bldr, ast->let.body, e);
    eth_pop_var(bldr->vars, nvars);
    return ret;
  }
}

// TODO: flag as likely
static eth_ir_node*
build_letrec(ir_builder *bldr, int idx, eth_ast *ast, int nvars0, int nvars,
    eth_ir_pattern *const pats[], int *e)
{
  if (idx < ast->letrec.n)
  {
    eth_ir_node *expr = build(bldr, ast->letrec.vals[idx], e);

    eth_ir_node *thenbr = build_letrec(bldr, idx + 1, ast, nvars0, nvars, pats, e);

    eth_ir_node *raise = eth_ir_cval(eth_get_builtin("raise"));
    eth_ir_node *what = eth_ir_cval(eth_str("type-error"));
    eth_ir_node *elsebr = eth_ir_apply(raise, &what, 1);

    eth_ir_node *ret = eth_ir_match(pats[idx], expr, thenbr, elsebr);
    ret->match.toplvl = ETH_TOPLVL_THEN;
    return ret;
  }
  else
  {
    nvars = bldr->nvars - nvars0;

    int varids[nvars];
    for (int i = 0; i < nvars; ++i)
      varids[i] = nvars0 + i;

    eth_ir_node *body = build(bldr, ast->letrec.body, e);
    eth_ir_node *ret = eth_ir_endfix(varids, nvars, body);

    eth_pop_var(bldr->vars, nvars);
    return ret;
  }
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
      bool tlvl = bldr->istoplvl;
      eth_ir_node *cond = build(bldr, ast->iff.cond, e);

      eth_ir_node *thenbr = build_with_toplvl(bldr, ast->iff.then, e,
          ast->iff.toplvl == ETH_TOPLVL_THEN);

      eth_ir_node *elsebr = build_with_toplvl(bldr, ast->iff.els, e,
          ast->iff.toplvl == ETH_TOPLVL_ELSE);

      eth_ir_node *ret = eth_ir_if(cond, thenbr, elsebr);
      ret->iff.toplvl = ast->iff.toplvl;
      return ret;
    }

    case ETH_AST_SEQ:
    {
      eth_ir_node *e1 = build(bldr, ast->seq.e1, e);
      eth_ir_node *e2 = build(bldr, ast->seq.e2, e);
      return eth_ir_seq(e1, e2);
    }

    case ETH_AST_LET:
    {
      eth_ir_node *vals[ast->let.n];
      for (int i = 0; i < ast->let.n; ++i)
        vals[i] = build(bldr, ast->let.vals[i], e);
      return build_let(bldr, 0, ast, vals, bldr->nvars, e);
    }

    case ETH_AST_LETREC:
    {
      eth_ir_pattern *pats[ast->letrec.n];

      int n1 = bldr->nvars;
      for (int i = 0; i < ast->letrec.n; ++i)
        pats[i] = build_pattern(bldr, ast->letrec.pats[i], e);
      int n2 = bldr->nvars;
      int nvars = n2 - n1;

      eth_ir_node *body = build_letrec(bldr, 0, ast, n1, nvars, pats, e);

      int varids[nvars];
      for (int i = 0; i < nvars; ++i)
        varids[i] = n1 + i;
      return eth_ir_startfix(varids, nvars, body);
    }

    case ETH_AST_BINOP:
    {
      eth_ir_node *lhs = build(bldr, ast->binop.lhs, e);
      eth_ir_node *rhs = build(bldr, ast->binop.rhs, e);
      return eth_ir_binop(ast->binop.op, lhs, rhs);
    }

    case ETH_AST_UNOP:
    {
      eth_ir_node *expr = build(bldr, ast->unop.expr, e);
      return eth_ir_unop(ast->unop.op, expr);
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

      eth_ir_node *thenbr = build_with_toplvl(bldr, ast->match.thenbr, e,
          ast->match.toplvl == ETH_TOPLVL_THEN);

      eth_pop_var(bldr->vars, nvars);

      eth_ir_node *elsebr = build_with_toplvl(bldr, ast->match.elsebr, e,
          ast->match.toplvl == ETH_TOPLVL_ELSE);

      eth_ir_node *ret = eth_ir_match(pat, expr, thenbr, elsebr);
      ret->match.toplvl = ast->match.toplvl;
      return ret;
    }

    case ETH_AST_IMPORT:
    {
      assert(bldr->env);
      eth_module *mod = eth_require_module(bldr->env, ast->import.module);
      if (not mod)
      {
        eth_warning("failed to import module %s", ast->import.module);
        *e = 1;
        return eth_ir_error();
      }
      int n1 = bldr->vars->len;
      int ok = load_module(bldr, mod, ast->import.alias, ast->import.nams,
          ast->import.nnam);
      if (not ok) *e = 1;
      int n2 = bldr->vars->len;
      eth_ir_node *body = build(bldr, ast->import.body, e);
      eth_pop_var(bldr->vars, n2 - n1);
      return body;
    }

    case ETH_AST_AND:
    {
      eth_ir_node *lhs = build(bldr, ast->scand.lhs, e);
      eth_ir_node *rhs = build(bldr, ast->scand.rhs, e);
      int tmpvar = new_vid(bldr);
      eth_ir_node *ret =
        eth_ir_bind(&tmpvar, &lhs, 1,
          eth_ir_if(eth_ir_var(tmpvar), rhs, eth_ir_cval(eth_false)));
      return ret;
    }

    case ETH_AST_OR:
    {
      eth_ir_node *lhs = build(bldr, ast->scand.lhs, e);
      eth_ir_node *rhs = build(bldr, ast->scand.rhs, e);
      int tmpvar = new_vid(bldr);
      eth_ir_node *ret =
        eth_ir_bind(&tmpvar, &lhs, 1,
          eth_ir_if(eth_ir_var(tmpvar), eth_ir_var(tmpvar), rhs));
      return ret;
    }

    case ETH_AST_TRY:
    {
      eth_ir_node *trybr = build(bldr, ast->try.trybr, e);

      int exnvar = new_vid(bldr);

      int n1 = bldr->vars->len;
      eth_ir_pattern *pat = build_pattern(bldr, ast->try.pat, e);
      int n2 = bldr->vars->len;
      int nvars = n2 - n1;
      eth_ir_node *ok = build(bldr, ast->try.catchbr, e);
      eth_pop_var(bldr->vars, n2 - n1);

      eth_ir_node *exn = eth_ir_var(exnvar);
      eth_ir_node *ris = eth_ir_cval(eth_get_builtin("raise"));
      eth_ir_node *rethrow = eth_ir_apply(ris, &exn, 1);

      eth_ir_node *catchbr = eth_ir_match(pat, eth_ir_var(exnvar), ok, rethrow);

      return eth_ir_try(exnvar, trybr, catchbr, ast->try.likely);
    }
  }

  eth_error("undefined AST-node");
  *e = 1;
  return eth_ir_error();
}

eth_ir*
eth_build_ir(eth_ast *ast, eth_env *env, eth_ir_defs *defs)
{
  eth_ir *ret;
  int e = 0;

  ir_builder *bldr = create_ir_builder(NULL);
  bldr->env = env;
  bldr->istoplvl = true;

  // import builtins:
  load_module(bldr, eth_builtins(), "", NULL, 0);
  // and extra modules:
  if (env)
  {
    int n = eth_get_nmodules(env);
    const eth_module *mods[n];
    eth_get_modules(env, mods, n);
    for (int i = 0; i < n; ++i)
      load_module(bldr, mods[i], NULL, NULL, 0);
  }

  eth_ir_node *body = build(bldr, ast, &e);
  if (e)
  {
    eth_drop_ir_node(body);
    ret = NULL;
    destroy_ir_builder(bldr);
    return NULL;
  }

  if (defs)
  {
    defs->idents = bldr->defidents.data;
    defs->varids = bldr->defvarids.data;
    defs->n = bldr->defidents.len;
    bldr->defidents.data = NULL;
    bldr->defvarids.data = NULL;
    cod_vec_destroy(bldr->defidents);
    cod_vec_destroy(bldr->defvarids);
  }

  ret = eth_create_ir(body, bldr->nvars);
  destroy_ir_builder(bldr);
  return ret;
}

void
eth_destroy_ir_defs(eth_ir_defs *defs)
{
  for (int i = 0; i < defs->n; ++i)
    free(defs->idents[i]);
  free(defs->idents);
  free(defs->varids);
}

