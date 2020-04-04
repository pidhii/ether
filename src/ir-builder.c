#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>


ETH_MODULE("ether:ir-builder")


typedef struct ir_builder {
  struct ir_builder *parent;
  eth_env *env; // top-level environment
  eth_module *mod; // this module
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
  bldr->mod = NULL;
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

static eth_type*
find_type(ir_builder *bldr, const char *name)
{
  for (size_t i = 0; i < bldr->types.len; ++i)
  {
    eth_type *ty = bldr->types.data[i];
    if (strcmp(ty->name, name) == 0)
      return ty;
  }
  return NULL;
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

// TODO: simplify (e.g. split on several specified functions)
static bool
load_module(ir_builder *bldr, const eth_module *mod, const char *alias,
    char *const nams[], int nnam)
{
  const char *modname = alias ? alias : eth_get_module_name(mod);
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
        eth_warning("no '%s' in module %s", nams[i], eth_get_module_name(mod));
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
    int ndefs = eth_get_ndefs(mod);
    eth_def defs[ndefs];
    eth_get_defs(mod, defs);
    for (int i = 0; i < ndefs; ++i)
    {
      char *ident = defs[i].ident;
      assert(strlen(ident) + sizeof prefix < sizeof buf);
      sprintf(buf, "%s%s", prefix, ident);
      eth_var_cfg var = eth_const_var(buf, defs[i].val);
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
      if (pat->unpack.isctype)
        type = pat->unpack.type.ctype;
      else
      {
        type = find_type(bldr, pat->unpack.type.str);
        if (type == NULL)
        {
          eth_error("no such type '%s'", pat->unpack.type.str);
          *e = 1;
          return eth_ir_ident_pattern(-1); // just some dummy
        }
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

    case ETH_PATTERN_SYMBOL:
      return eth_ir_symbol_pattern(pat->symbol.sym);

    case ETH_PATTERN_RECORD:
    {
      int n = pat->record.n;
      size_t ids[n];
      eth_ir_pattern *pats[n];
      for (int i = 0; i < n; ++i)
      {
        ids[i] = eth_get_symbol_id(eth_sym(pat->record.fields[i]));
        pats[i] = build_pattern(bldr, pat->record.subpats[i], e);
      }
      return eth_ir_record_pattern(ids, pats, n);
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

    eth_t exn = eth_exn(eth_sym("Match_failure"));
    eth_ir_node *elsebr = eth_ir_throw(eth_ir_cval(exn));

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
    eth_ir_node *what = eth_ir_cval(eth_sym("Match_failure"));
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

eth_ir_node*
constexpr_binop(eth_binop op, eth_t lhs, eth_t rhs, int *e)
{
  eth_t ret = NULL;
  switch (op)
  {
    case ETH_ADD ... ETH_POW:
    case ETH_LAND ... ETH_ASHR:
    case ETH_LT ... ETH_NE:
    {
      if (lhs->type != eth_number_type || rhs->type != eth_number_type)
      {
        eth_warning("invalid operands for binary %s (%s and %s)",
            eth_binop_sym(op), lhs->type->name, rhs->type->name);
        *e = 1;
        return eth_ir_error();
      }
      eth_number_t x = ETH_NUMBER(lhs)->val;
      eth_number_t y = ETH_NUMBER(rhs)->val;
      switch (op)
      {
        case ETH_ADD: ret = eth_num(x + y); break;
        case ETH_SUB: ret = eth_num(x - y); break;
        case ETH_MUL: ret = eth_num(x * y); break;
        case ETH_DIV: ret = eth_num(x / y); break;
        case ETH_MOD: ret = eth_num(eth_mod(x, y)); break;
        case ETH_POW: ret = eth_num(eth_pow(x, y)); break;
        // ---
        case ETH_LAND: ret = eth_num((intmax_t)x & (intmax_t)y); break;
        case ETH_LOR:  ret = eth_num((intmax_t)x | (intmax_t)y); break;
        case ETH_LXOR: ret = eth_num((intmax_t)x ^ (intmax_t)y); break;
        case ETH_LSHL: ret = eth_num((uintmax_t)x << (uintmax_t)y); break;
        case ETH_LSHR: ret = eth_num((uintmax_t)x >> (uintmax_t)y); break;
        case ETH_ASHL: ret = eth_num((intmax_t)x << (intmax_t)y); break;
        case ETH_ASHR: ret = eth_num((intmax_t)x >> (intmax_t)y); break;
        // ---
        case ETH_LT: ret = eth_boolean(lhs < rhs); break;
        case ETH_LE: ret = eth_boolean(lhs <= rhs); break;
        case ETH_GT: ret = eth_boolean(lhs > rhs); break;
        case ETH_GE: ret = eth_boolean(lhs >= rhs); break;
        case ETH_EQ: ret = eth_boolean(lhs == rhs); break;
        case ETH_NE: ret = eth_boolean(lhs != rhs); break;
        // ---
        default: abort();
      }
      break;
    }

    case ETH_IS: ret = eth_boolean(lhs == rhs); break;
    case ETH_CONS: ret = eth_cons(lhs, rhs); break;
  }

  return eth_ir_cval(ret);
}

static eth_ir_node*
constexpr_unop(eth_unop op, eth_t x, int *e)
{
  eth_t ret = NULL;
  switch (op)
  {
    case ETH_NOT: ret = eth_boolean(x == eth_false); break;
    case ETH_LNOT: ret = eth_num(~(intmax_t)ETH_NUMBER(x)->val);
  }
  return eth_ir_cval(ret);
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
      eth_ir_node *ret;
      if (lhs->tag == ETH_IR_CVAL and rhs->tag == ETH_IR_CVAL)
      {
        ret = constexpr_binop(ast->binop.op, lhs->cval.val, rhs->cval.val, e);
        eth_drop_ir_node(lhs);
        eth_drop_ir_node(rhs);
      }
      else
        ret = eth_ir_binop(ast->binop.op, lhs, rhs);
      return ret;
    }

    case ETH_AST_UNOP:
    {
      eth_ir_node *expr = build(bldr, ast->unop.expr, e);
      eth_ir_node *ret;
      if (expr->tag == ETH_IR_CVAL)
      {
        ret = constexpr_unop(ast->unop.op, expr->cval.val, e);
        eth_drop_ir_node(expr);
      }
      else
        ret = eth_ir_unop(ast->unop.op, expr);
      return ret;
    }

    case ETH_AST_FN:
    {
      ir_builder *fnbldr = create_ir_builder(bldr);

      // arguments:
      int nargs = ast->fn.arity;
      int args[nargs];
      eth_ir_pattern *pats[nargs];
      // Note: arguments MUST be continuous and start at 0:
      for (int i = 0; i < nargs; ++i)
        args[i] = new_vid(fnbldr);
      for (int i = 0; i < nargs; ++i)
        pats[i] = build_pattern(fnbldr, ast->fn.args[i], e);
      int nargvars = fnbldr->vars->len;

      // body:
      eth_ir_node *body_acc = build(fnbldr, ast->fn.body, e);
      eth_ir_node *throw = NULL;
      for (int i = nargs - 1; i >= 0; --i)
      {
        if (throw == NULL)
        {
          eth_t exn = eth_exn(eth_sym("Invalid_argument"));
          throw = eth_ir_throw(eth_ir_cval(exn));
        }
        // TODO: mark as likely
        body_acc = eth_ir_match(pats[i], eth_ir_var(args[i]), body_acc, throw);
      }
      eth_ir *body = eth_create_ir(body_acc, fnbldr->nvars);

      // resolve captures:
      eth_pop_var(fnbldr->vars, nargvars); // pop args so only captures are left
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

      // 1. try to load as a submodule
      // 2. otherwize, load with global environment
      eth_module *mod = NULL;
      if (bldr->mod)
        mod = eth_require_module(eth_get_env(bldr->mod), ast->import.module);
      if (not mod)
        mod = eth_require_module(bldr->env, ast->import.module);

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

    case ETH_AST_MKRCRD:
    {
      eth_type *type;
      if (not ast->mkrcrd.isctype)
      {
        type = find_type(bldr, ast->mkrcrd.type.str);
        if (type == NULL)
        {
          eth_error("no such type '%s'", ast->mkrcrd.type.str);
          *e = 1;
          return eth_ir_error();
        }
      }
      else
        type = ast->mkrcrd.type.ctype;

      if (not eth_is_plain(type))
      {
        eth_error("%s is not a plain type", type->name);
        *e = 1;
        return eth_ir_error();
      }

      int nsup = ast->mkrcrd.n;
      int ntot = type->nfields;
      char *fields[ntot];
      memcpy(fields, ast->mkrcrd.fields, sizeof(char*) * nsup);
      typedef struct { eth_ast *val; char *field; ptrdiff_t offs; } value;
      value vals[ntot];

      // validate supplied fields
      for (int i = 0; i < nsup; ++i)
      {
        eth_field *fldinfo = eth_get_field(type, fields[i]);
        if (not fldinfo)
        {
          eth_error("no field '%s' in record %s", fields[i],
              ast->mkrcrd.type.str);
          *e = 1;
          return eth_ir_error();
        }
        vals[i].val = ast->mkrcrd.vals[i];
        vals[i].field = ast->mkrcrd.fields[i];
        vals[i].offs = fldinfo->offs;
      }
      // add missing fields
      if (nsup != ntot)
      {
        eth_error("default values for fields are not implemented");
        *e = 1;
        return eth_ir_error();
      }

      // sort fields
      bool err = false;
      int cmp(const void *p1, const void *p2)
      {
        const value *v1 = p1;
        const value *v2 = p2;
        return v1->offs - v2->offs;
      }
      qsort(vals, ntot, sizeof(value), cmp);

      eth_ir_node *irvals[ntot];
      for (int i = 0; i < ntot; ++i)
        irvals[i] = build(bldr, vals[i].val, e);
      return eth_ir_mkrcrd(type, irvals);
    }
  }

  eth_error("undefined AST-node");
  *e = 1;
  return eth_ir_error();
}

static eth_ir*
build_ir(eth_ast *ast, eth_env *env, eth_module *mod, eth_ir_defs *defs)
{
  eth_ir *ret;
  int e = 0;

  ir_builder *bldr = create_ir_builder(NULL);
  bldr->env = env;
  bldr->istoplvl = true;
  bldr->mod = mod;

  // import builtins:
  load_module(bldr, eth_builtins(), "", NULL, 0);

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

eth_ir*
eth_build_ir(eth_ast *ast, eth_env *env)
{
  return build_ir(ast, env, NULL, NULL);
}

eth_ir*
eth_build_module_ir(eth_ast *ast, eth_env *env, eth_module *mod,
    eth_ir_defs *defs)
{
  return build_ir(ast, env, mod, defs);
}

void
eth_destroy_ir_defs(eth_ir_defs *defs)
{
  for (int i = 0; i < defs->n; ++i)
    free(defs->idents[i]);
  free(defs->idents);
  free(defs->varids);
}

