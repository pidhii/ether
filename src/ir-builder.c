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
#include "codeine/vec.h"
#include "ether/ether.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

ETH_MODULE("ether:ir-builder")

typedef struct {
  char *name;
  const eth_module *module;
} module_entry;

typedef struct ir_builder {
  struct ir_builder *parent;
  eth_root *root;           // top-level environment
  eth_module *mod;          // this module
  bool istoplvl;            // whether it is a top-level scope
  eth_var_list *vars;       // associative list (alist) of variables
  int capoffs;              // offset of captured variables in the alist
  int nvars;                // number of IR variables
  cod_vec(eth_ir_def) defs; // container for exports
  int immcnt;               // counter for unique names for macro-modules
} ir_builder;

static ir_builder *
create_ir_builder(ir_builder *parent)
{
  ir_builder *bldr = eth_malloc(sizeof(ir_builder));
  bldr->parent = parent;
  bldr->mod = NULL;
  bldr->root = parent ? parent->root : NULL;
  bldr->istoplvl = false;
  bldr->vars = eth_create_var_list();
  bldr->capoffs = 0;
  bldr->nvars = 0;
  bldr->immcnt = 0;
  cod_vec_init(bldr->defs);

  return bldr;
}

static void
destroy_ir_builder(ir_builder *bldr)
{
  eth_destroy_var_list(bldr->vars);
  cod_vec_iter(bldr->defs, i, x, {
    if (x.tag == ETH_IRDEF_CVAL)
      eth_unref(x.cval);
    free(x.ident);
    eth_unref_attr(x.attr);
  });
  cod_vec_destroy(bldr->defs);
  free(bldr);
}

static eth_ir_node *
build(ir_builder *bldr, eth_ast *ast, int *e);

static eth_ir_node *
build_with_toplvl(ir_builder *bldr, eth_ast *ast, int *e, bool savetop);

static inline int
new_vid(ir_builder *bldr)
{
  return bldr->nvars++;
}

static inline void
trace_pub_var(ir_builder *bldr, const char *ident, int varid, eth_attr *attr,
              eth_location *loc, int *e)
{
  if (bldr->istoplvl)
  {
    eth_ir_def def = {
        .tag = ETH_IRDEF_VAR,
        .ident = strdup(ident),
        .attr = attr,
        .varid = varid,
    };
    cod_vec_push(bldr->defs, def);
    eth_ref_attr(attr);
  }
  else
  {
    eth_warning("can't have public variables outside top level-scope");
    *e = 1;
    eth_print_location(loc, stderr);
  }
}

static inline void
trace_pub_cval(ir_builder *bldr, const char *ident, eth_t cval, eth_attr *attr,
               eth_location *loc, int *e)
{
  if (bldr->istoplvl)
  {
    eth_ir_def def = {
        .tag = ETH_IRDEF_CVAL,
        .ident = strdup(ident),
        .attr = attr,
        .cval = cval,
    };
    cod_vec_push(bldr->defs, def);
    eth_ref(cval);
    eth_ref_attr(attr);
  }
  else
  {
    eth_warning("can't have public variables outside top level-scope");
    *e = 1;
    eth_print_location(loc, stderr);
  }
}

/*
 * Require variable in the scope. Variable will be captured if not found in
 * current scope.
 *
 * Note: Constants wont be captured. If constant variable is returned, it's
 * constant values must be used instead.
 */
static bool
require_var(ir_builder *bldr, const char *ident, eth_var *ret)
{
  eth_var *localvar = eth_find_var(bldr->vars->head, ident, NULL);
  if (localvar)
  {
    *ret = *localvar;
    return true;
  }
  else if (bldr->parent)
  {
    if (require_var(bldr->parent, ident, ret))
    {
      // immediately return constants (don't perform capture)
      if (ret->cval)
        return true;
      int vid = new_vid(bldr);
      bldr->capoffs += 1;
      *ret = *eth_append_var(bldr->vars, eth_copy_var(ret, vid));
      return true;
    }
  }
  return false;
}

static eth_var *
find_var_deep(ir_builder *bldr, const char *ident)
{
  eth_var *var = eth_find_var(bldr->vars->head, ident, NULL);
  if (var)
    return var;
  else if (bldr->parent)
    return find_var_deep(bldr->parent, ident);
  else
    return NULL;
}

static void
import(ir_builder *bldr, const eth_module *mod)
{
  int ndefs = eth_get_ndefs(mod);
  eth_def defs[ndefs];
  eth_get_defs(mod, defs);
  for (int i = 0; i < ndefs; ++i)
  {
    eth_var_cfg varcfg =
        eth_const_var(defs[i].ident, defs[i].val, defs[i].attr);
    eth_prepend_var(bldr->vars, varcfg);
  }
}

static eth_ir_pattern *
build_pattern(ir_builder *bldr, eth_ast_pattern *pat, eth_location *loc, int *e)
{
  switch (pat->tag)
  {
    case ETH_AST_PATTERN_DUMMY:
      return eth_ir_dummy_pattern();

    case ETH_AST_PATTERN_IDENT: {
      int varid = new_vid(bldr);

      // set up attribute
      eth_attr *attr = NULL;
      if (pat->ident.attr)
      {
        // handle PUB
        attr = pat->ident.attr;
        if (pat->ident.attr->flag & ETH_ATTR_PUB)
          trace_pub_var(bldr, pat->ident.str, varid, attr, loc, e);
        /*else*/
        /*eth_drop_attr(attr);*/
      }

      eth_prepend_var(bldr->vars, eth_dyn_var(pat->ident.str, varid, attr));
      return eth_ir_ident_pattern(varid);
    }

    case ETH_AST_PATTERN_UNPACK: {
      eth_type *type = pat->unpack.type;

      // aliasing:
      int alias = new_vid(bldr);
      if (pat->unpack.alias)
        eth_prepend_var(bldr->vars,
                        eth_dyn_var(pat->unpack.alias, alias, NULL));

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
          eth_print_location(loc, stderr);
          return eth_ir_ident_pattern(-1); // just some dummy
        }
        offs[i] = fld->offs;
        pats[i] = build_pattern(bldr, pat->unpack.subpats[i], loc, e);
      }
      return eth_ir_unpack_pattern(alias, type, offs, pats, n);
    }

    case ETH_AST_PATTERN_CONSTANT:
      return eth_ir_constant_pattern(pat->constant.val);

    // TODO: handle prototype check when compile-time info on input is available
    case ETH_AST_PATTERN_RECORD: {
      // aliasing:
      int alias = new_vid(bldr);
      if (pat->record.alias)
        eth_prepend_var(bldr->vars,
                        eth_dyn_var(pat->record.alias, alias, NULL));

      int n = pat->record.n;
      typedef struct {
        uintptr_t id;
        eth_ir_pattern *pat;
      } field;
      field fields[n];

      for (int i = 0; i < n; ++i)
      {
        fields[i].id = eth_get_symbol_id(eth_sym(pat->record.fields[i]));
        fields[i].pat = build_pattern(bldr, pat->record.subpats[i], loc, e);
      }
      int cmp(const void *p1, const void *p2)
      {
        const field *f1 = p1;
        const field *f2 = p2;
        return f1->id - f2->id;
      }
      qsort(fields, n, sizeof(field), cmp);

      size_t ids[n];
      eth_ir_pattern *pats[n];
      for (int i = 0; i < n; ++i)
      {
        ids[i] = fields[i].id;
        pats[i] = fields[i].pat;
      }

      return eth_ir_record_pattern(alias, ids, pats, n);
    }

    case ETH_AST_PATTERN_RECORD_STAR: {
      eth_error("wild record, {*}, is not allowed in this context");
      *e = 1;
      eth_print_location(loc, stderr);
      return eth_ir_ident_pattern(-1); // just some dummy
    }

    default:
      eth_error("unreachable case");
      abort();
  }
}

static bool
build_record_star(ir_builder *bldr, eth_ast_pattern *pat, eth_location *loc,
                  eth_ir_node *expr, int *e)
{
  if (expr->tag != ETH_IR_CVAL)
  {
    eth_error("wild record is only allowed with constant expressions");
    eth_print_location(loc, stderr);
    *e = 1;
    return NULL;
  }

  eth_t record = expr->cval.val;
  if (not eth_is_record(record->type))
  {
    eth_error("target expression is not a record");
    eth_print_location(loc, stderr);
    *e = 1;
    return NULL;
  }

  int n = eth_struct_size(record->type);
  for (int i = n - 1; i >= 0; --i)
  {
    const char *key = record->type->fields[i].name;
    eth_t val = eth_tup_get(record, i);

    // set up attribute
    eth_attr *attr = NULL;
    if (pat->recordstar.attr)
    {
      // handle PUB
      attr = pat->recordstar.attr;
      if (pat->recordstar.attr->flag & ETH_ATTR_PUB)
        trace_pub_cval(bldr, key, val, attr, loc, e);
      /*else*/
      /*eth_drop_attr(attr);*/
    }
    eth_prepend_var(bldr->vars, eth_const_var(key, val, attr));
  }

  if (pat->recordstar.alias)
  {
    /*eth_trace("alias {*} (%s)", pat->recordstar.alias);*/
    eth_prepend_var(bldr->vars,
                    eth_const_var(pat->recordstar.alias, record, NULL));
  }

  return true;
}

static bool
build_pattern_constexpr(ir_builder *bldr, eth_ast_pattern *pat, eth_t expr,
                        eth_location *loc, int *e)
{
  switch (pat->tag)
  {
    case ETH_AST_PATTERN_DUMMY:
      return true;

    case ETH_AST_PATTERN_IDENT: {
      // set up attribute
      eth_attr *attr = NULL;
      if (pat->ident.attr)
      {
        // handle PUB
        attr = pat->ident.attr;
        if (pat->ident.attr->flag & ETH_ATTR_PUB)
          trace_pub_cval(bldr, pat->ident.str, expr, attr, loc, e);
      }

      eth_prepend_var(bldr->vars, eth_const_var(pat->ident.str, expr, attr));
      return true;
    }

    case ETH_AST_PATTERN_UNPACK: {
      eth_type *type = pat->unpack.type;
      if (expr->type != type)
        return false;

      // aliasing:
      if (pat->unpack.alias)
        eth_prepend_var(bldr->vars,
                        eth_const_var(pat->unpack.alias, expr, NULL));

      int n = pat->unpack.n;
      int offs[n];
      eth_ir_pattern *pats[n];
      for (int i = 0; i < n; ++i)
      {
        eth_field *fld = eth_get_field(type, pat->unpack.fields[i]);
        eth_t val = *(eth_t *)((char *)expr + fld->offs);
        if (not build_pattern_constexpr(bldr, pat->unpack.subpats[i], val, loc,
                                        e))
          return false;
      }
      return true;
    }

    case ETH_AST_PATTERN_CONSTANT:
      return eth_equal(expr, pat->constant.val);

    case ETH_AST_PATTERN_RECORD: {
      if (not eth_is_record(expr->type))
        return false;

      // aliasing:
      if (pat->record.alias)
        eth_prepend_var(bldr->vars,
                        eth_const_var(pat->record.alias, expr, NULL));

      int n = pat->record.n;
      for (int i = 0; i < n; ++i)
      {
        eth_field *fld = eth_get_field(expr->type, pat->record.fields[i]);
        if (fld == NULL)
        {
          eth_error("no field '%s' in type '%s'\n", pat->record.fields[i],
                    expr->type->name);
          *e = 1;
          return false;
        }

        eth_t val = *(eth_t *)((char *)expr + fld->offs);
        if (not build_pattern_constexpr(bldr, pat->record.subpats[i], val, loc,
                                        e))
          return false;
      }

      return true;
    }

    case ETH_AST_PATTERN_RECORD_STAR: {
      eth_ir_node *ex = eth_ir_cval(expr);
      eth_ref_ir_node(ex);
      if (not build_record_star(bldr, pat, loc, ex, e))
      {
        eth_unref_ir_node(ex);
        return false;
      }
      eth_unref_ir_node(ex);
      return true;
    }

    default:
      eth_error("unreachable case");
      abort();
  }
}

static eth_ir_node *
build_with_toplvl(ir_builder *bldr, eth_ast *ast, int *e, bool savetop)
{
  bool tlvl = bldr->istoplvl;
  if (not savetop)
    bldr->istoplvl = false;
  eth_ir_node *ret = build(bldr, ast, e);
  bldr->istoplvl = tlvl;
  return ret;
}

static eth_ir_node *
build_let(ir_builder *bldr, int idx, const eth_ast *ast,
          eth_ir_node *const vals[], int nvars0, int *e)
{
  if (idx < ast->let.n)
  {
    if (vals[idx]->tag == ETH_IR_CVAL)
    {
      bool ok = build_pattern_constexpr(bldr, ast->let.pats[idx],
                                        vals[idx]->cval.val, ast->loc, e);
      if (not ok)
      {
        eth_warning("pattern in LET-expression wount match to `~w`",
                    vals[idx]->cval.val);
        eth_drop_ir_node(vals[idx]);
        *e = 1;
        eth_print_location(ast->loc, stderr);
        return eth_ir_error();
      }
      eth_drop_ir_node(vals[idx]);
      return build_let(bldr, idx + 1, ast, vals, nvars0, e);
    }
    else
    {
      eth_ir_pattern *pat =
          build_pattern(bldr, ast->let.pats[idx], ast->loc, e);

      eth_ir_node *thenbr = build_let(bldr, idx + 1, ast, vals, nvars0, e);

      eth_t exn = eth_exn(eth_type_error());
      eth_ir_node *elsebr = eth_ir_throw(eth_ir_cval(exn));
      eth_set_ir_location(elsebr, ast->let.vals[idx]->loc);

      eth_ir_node *ret = eth_ir_match(pat, vals[idx], thenbr, elsebr);
      eth_set_ir_location(ret, ast->loc);
      ret->match.toplvl = bldr->istoplvl ? ETH_TOPLVL_THEN : ETH_TOPLVL_NONE;
      ret->match.likely = 1;
      return ret;
    }
  }
  else
  {
    int nvars = bldr->vars->len - bldr->capoffs - nvars0;
    eth_ir_node *ret = build(bldr, ast->let.body, e);
    eth_pop_var(bldr->vars, nvars);
    return ret;
  }
}

static bool
is_binop_redefined(ir_builder *bldr, eth_binop op)
{
  eth_var *var = find_var_deep(bldr, eth_binop_sym(op));
  if (var == NULL)
    return false;
  else if (var->attr && (var->attr->flag & ETH_ATTR_BUILTIN))
    return false;
  else
    return true;
}

static bool
is_unop_redefined(ir_builder *bldr, eth_unop op)
{
  eth_var *var = find_var_deep(bldr, eth_unop_sym(op));
  if (var == NULL)
    return false;
  if (var->attr && (var->attr->flag & ETH_ATTR_BUILTIN))
    return false;
  else
    return true;
}

static bool
is_redefined(ir_builder *bldr, const char *ident)
{
  eth_var *var = find_var_deep(bldr, ident);
  if (var == NULL)
    return false;
  else if (var->attr && (var->attr->flag & ETH_ATTR_BUILTIN))
    return false;
  else
    return true;
}

static eth_ir_node *
constexpr_binop(eth_binop op, eth_t lhs, eth_t rhs, eth_location *loc, int *e)
{
  eth_t ret = NULL;
  switch (op)
  {
    case ETH_ADD ... ETH_POW:
    case ETH_LAND ... ETH_ASHR:
    case ETH_LT ... ETH_NE: {
      if (lhs->type != eth_number_type || rhs->type != eth_number_type)
      {
        eth_warning("invalid operands for binary %s (%s and %s)",
                    eth_binop_sym(op), lhs->type->name, rhs->type->name);
        *e = 1;
        eth_print_location(loc, stderr);
        return eth_ir_error();
      }
      eth_number_t x = ETH_NUMBER(lhs)->val;
      eth_number_t y = ETH_NUMBER(rhs)->val;
      switch (op)
      {
        case ETH_ADD:
          ret = eth_num(x + y);
          break;
        case ETH_SUB:
          ret = eth_num(x - y);
          break;
        case ETH_MUL:
          ret = eth_num(x * y);
          break;
        case ETH_DIV:
          ret = eth_num(x / y);
          break;
        case ETH_MOD:
          ret = eth_num(eth_mod(x, y));
          break;
        case ETH_POW:
          ret = eth_num(eth_pow(x, y));
          break;
        // ---
        case ETH_LAND:
          ret = eth_num((intmax_t)x & (intmax_t)y);
          break;
        case ETH_LOR:
          ret = eth_num((intmax_t)x | (intmax_t)y);
          break;
        case ETH_LXOR:
          ret = eth_num((intmax_t)x ^ (intmax_t)y);
          break;
        case ETH_LSHL:
          ret = eth_num((uintmax_t)x << (uintmax_t)y);
          break;
        case ETH_LSHR:
          ret = eth_num((uintmax_t)x >> (uintmax_t)y);
          break;
        case ETH_ASHL:
          ret = eth_num((intmax_t)x << (intmax_t)y);
          break;
        case ETH_ASHR:
          ret = eth_num((intmax_t)x >> (intmax_t)y);
          break;
        // ---
        case ETH_LT:
          ret = x < y ? eth_num(y) : eth_false;
          break;
        case ETH_LE:
          ret = x <= y ? eth_num(y) : eth_false;
          break;
        case ETH_GT:
          ret = x > y ? eth_num(y) : eth_false;
          break;
        case ETH_GE:
          ret = x >= y ? eth_num(y) : eth_false;
          break;
        case ETH_EQ:
          ret = eth_boolean(x == y);
          break;
        case ETH_NE:
          ret = eth_boolean(x != y);
          break;
        // ---
        default:
          abort();
      }
      break;
    }

    case ETH_IS:
      ret = eth_boolean(lhs == rhs);
      break;
    case ETH_EQUAL:
      ret = eth_boolean(eth_equal(lhs, rhs));
      break;
    case ETH_CONS:
      ret = eth_cons(lhs, rhs);
      break;
  }

  return eth_ir_cval(ret);
}

static eth_ir_node *
constexpr_unop(eth_unop op, eth_t x, int *__attribute((unused)) e)
{
  eth_t ret = NULL;
  switch (op)
  {
    case ETH_NOT:
      ret = eth_boolean(x == eth_false);
      break;
    case ETH_LNOT:
      ret = eth_num(~(intmax_t)ETH_NUMBER(x)->val);
  }
  return eth_ir_cval(ret);
}

static eth_ir_node *
build_fn(ir_builder *bldr, eth_ast *fnast, char *scpvarnams[], int nscpvars,
         int *e)
{
  ir_builder *fnbldr = create_ir_builder(bldr);

  // arguments:
  int nargs = fnast->fn.arity;
  int args_local[nargs];
  eth_ir_pattern *pats[nargs];
  // Note: arguments MUST be continuous and start at 0:
  for (int i = 0; i < nargs; ++i)
    args_local[i] = new_vid(fnbldr);
  for (int i = 0; i < nargs; ++i)
    pats[i] = build_pattern(fnbldr, fnast->fn.args[i], fnast->loc, e);
  int nargvars = fnbldr->vars->len;

  // declare recursive scope vars
  int scpvars_local[nscpvars];
  for (int i = 0; i < nscpvars; ++i)
  {
    scpvars_local[i] = new_vid(fnbldr);
    eth_prepend_var(fnbldr->vars,
                    eth_dyn_var(scpvarnams[i], scpvars_local[i], NULL));
  }

  // body:
  eth_ir_node *body_acc = build_with_toplvl(fnbldr, fnast->fn.body, e, false);
  eth_ir_node *throw = eth_ir_throw(eth_ir_cval(eth_exn(eth_type_error())));
  for (int i = nargs - 1; i >= 0; --i)
  {
    body_acc =
        eth_ir_match(pats[i], eth_ir_var(args_local[i]), body_acc, throw);
    body_acc->match.likely = 1;
  }
  eth_drop_ir_node(throw);
  eth_ir *body = eth_create_ir(body_acc, fnbldr->nvars);

  // resolve captures:
  eth_pop_var(fnbldr->vars, nscpvars); // pop scope vars => args and captures
                                       // are left
  eth_pop_var(fnbldr->vars, nargvars); // pop args => only captures are left
  int ncap = fnbldr->vars->len;
  int capvars_parent[ncap];
  int capvars_local[ncap];
  int i = 0;
  for (eth_var *cap = fnbldr->vars->head; i < ncap; cap = cap->next, ++i)
  {
    assert(cap->vid >= 0);
    capvars_local[i] = cap->vid;

    eth_var *var = eth_find_var(bldr->vars->head, cap->ident, NULL);
    assert(var && var->vid >= 0);
    capvars_parent[i] = var->vid;
  }

  destroy_ir_builder(fnbldr);
  return eth_ir_fn(fnast->fn.arity, capvars_parent, capvars_local, ncap,
                   scpvars_local, nscpvars, body, fnast);
}

static eth_ir_node *
try_resolve_ident(ir_builder *bldr, const char *str, eth_location *loc)
{
  eth_var var;
  if (not require_var(bldr, str, &var))
    return NULL;

  if (var.attr && var.attr->flag & ETH_ATTR_DEPRECATED)
  {
    eth_warning("use of deprecated variable, '%s'", str);
    if (loc)
      eth_print_location_opt(loc, stderr, ETH_LOPT_FILE);
  }

  if (var.cval)
  {
    return eth_ir_cval(var.cval);
  }
  else if (var.attr && var.attr->flag & ETH_ATTR_MUT)
  {
    eth_t deref = eth_get_builtin(bldr->root, "__dereference");
    eth_ir_node *args[] = {eth_ir_var(var.vid)};
    return eth_ir_apply(eth_ir_cval(deref), args, 1);
  }
  else
  {
    return eth_ir_var(var.vid);
  }
}

static eth_ir_node *
build(ir_builder *bldr, eth_ast *ast, int *e)
{
  switch (ast->tag)
  {
    case ETH_AST_CVAL: {
      return eth_ir_cval(ast->cval.val);
    }

    case ETH_AST_IDENT: {
      eth_ir_node *ret;
      if ((ret = try_resolve_ident(bldr, ast->ident.str, ast->loc)))
        return ret;
      else
      {
        eth_error("undefined variable, '%s'", ast->ident.str);
        *e = 1;
        eth_print_location(ast->loc, stderr);
        return eth_ir_error();
      }
    }

    case ETH_AST_APPLY: {
      eth_ir_node *fn = build_with_toplvl(bldr, ast->apply.fn, e, false);
      eth_ir_node *args[ast->apply.nargs];
      for (int i = 0; i < ast->apply.nargs; ++i)
        args[i] = build_with_toplvl(bldr, ast->apply.args[i], e, false);
      eth_ir_node *ret = eth_ir_apply(fn, args, ast->apply.nargs);
      eth_set_ir_location(ret, ast->loc);
      return ret;
    }

    case ETH_AST_IF: {
      eth_ir_node *cond = build_with_toplvl(bldr, ast->iff.cond, e, false);
      eth_ast *thenast = ast->iff.then;
      eth_ast *elseast = ast->iff.els ? ast->iff.els : eth_ast_cval(eth_nil);
      eth_ref_ast(thenast);
      eth_ref_ast(elseast);

      if (cond->tag == ETH_IR_CVAL)
      {
        eth_ir_node *ret;
        if (cond->cval.val != eth_false)
          ret = build_with_toplvl(bldr, thenast, e, false);
        else
          ret = build_with_toplvl(bldr, elseast, e, false);
        eth_drop_ir_node(cond);
        eth_unref_ast(thenast);
        eth_unref_ast(elseast);
        return ret;
      }
      else
      {
        eth_ir_node *thenbr = build_with_toplvl(bldr, thenast, e, false);
        eth_ir_node *elsebr = build_with_toplvl(bldr, elseast, e, false);
        eth_unref_ast(thenast);
        eth_unref_ast(elseast);
        return eth_ir_if(cond, thenbr, elsebr);
      }
    }

    case ETH_AST_SEQ: {
      eth_ir_node *e1 = build_with_toplvl(bldr, ast->seq.e1, e, false);
      eth_ir_node *e2 = build(bldr, ast->seq.e2, e);
      return eth_ir_seq(e1, e2);
    }

    case ETH_AST_LET: {
      eth_ir_node *vals[ast->let.n];
      for (int i = 0; i < ast->let.n; ++i)
        vals[i] = build_with_toplvl(bldr, ast->let.vals[i], e, false);
      return build_let(bldr, 0, ast, vals, bldr->vars->len - bldr->capoffs, e);
    }

    case ETH_AST_LETREC: {
      const int n = ast->letrec.n;

      // validate expressions: only closures allowed
      // and collect scope vars
      char *scpvarnams[n];
      for (int i = 0; i < n; ++i)
      {
        if (ast->letrec.vals[i]->tag != ETH_AST_FN)
        {
          eth_error("Only closures allowed in recursive scope");
          eth_print_location(ast->loc, stderr);
          *e = true;
          return eth_ir_error();
        }

        if (ast->letrec.pats[i]->tag != ETH_AST_PATTERN_IDENT)
        {
          eth_error("Only closures allowed in recursive scope (thus can't match"
                    " non-symbol pattern)");
          eth_print_location(ast->loc, stderr);
          *e = true;
          return eth_ir_error();
        }

        scpvarnams[i] = ast->letrec.pats[i]->ident.str;
      }

      // build scope vars
      eth_ir_node *fns[n];
      for (int i = 0; i < n; ++i)
        fns[i] = build_fn(bldr, ast->letrec.vals[i], scpvarnams, n, e);

      // declare scope vars
      int varids[n];
      for (int i = 0; i < n; ++i)
      {
        varids[i] = new_vid(bldr);
        eth_prepend_var(bldr->vars,
                        eth_dyn_var(scpvarnams[i], varids[i], NULL));
        eth_attr *attr = ast->letrec.pats[i]->ident.attr;
        if (attr)
        { // handle PUB
          if (attr->flag & ETH_ATTR_PUB)
            trace_pub_var(bldr, scpvarnams[i], varids[i], attr, ast->loc, e);
        }
      }

      // build body
      eth_ir_node *body = build(bldr, ast->letrec.body, e);

      // pop scope vars
      eth_pop_var(bldr->vars, n);

      return eth_ir_letrec(varids, fns, n, body);
    }

    case ETH_AST_BINOP: {
      if (is_binop_redefined(bldr, ast->binop.op))
      { // user-defined operator -> build as application
        eth_debug("redefined operator '%s'", eth_binop_sym(ast->binop.op));
        eth_ast *f = eth_ast_ident(eth_binop_sym(ast->binop.op));
        eth_ast *xs[] = {ast->binop.lhs, ast->binop.rhs};
        eth_ast *apply = eth_ast_apply(f, xs, 2);
        eth_set_ast_location(apply, ast->loc);
        eth_ir_node *ret = build_with_toplvl(bldr, apply, e, false);
        eth_drop_ast(apply);
        return ret;
      }
      else
      { // builtin binary operator:
        eth_ir_node *lhs = build_with_toplvl(bldr, ast->binop.lhs, e, false);
        eth_ir_node *rhs = build_with_toplvl(bldr, ast->binop.rhs, e, false);
        eth_ir_node *ret;
        if (lhs->tag == ETH_IR_CVAL and rhs->tag == ETH_IR_CVAL)
        {
          ret = constexpr_binop(ast->binop.op, lhs->cval.val, rhs->cval.val,
                                ast->loc, e);
          eth_drop_ir_node(lhs);
          eth_drop_ir_node(rhs);
        }
        else
        {
          ret = eth_ir_binop(ast->binop.op, lhs, rhs);
          eth_set_ir_location(ret, ast->loc);
        }
        return ret;
      }
    }

    case ETH_AST_UNOP: {
      if (is_unop_redefined(bldr, ast->unop.op))
      { // user-defined operator -> build as application
        eth_debug("redefined operator '%s'", eth_unop_sym(ast->unop.op));
        eth_ast *f = eth_ast_ident(eth_unop_sym(ast->unop.op));
        eth_ast *xs[] = {
            ast->unop.expr,
        };
        eth_ast *apply = eth_ast_apply(f, xs, 1);
        eth_set_ast_location(apply, ast->loc);
        eth_ir_node *ret = build_with_toplvl(bldr, apply, e, false);
        eth_drop_ast(apply);
        return ret;
      }
      else
      { // builtin unary operator:
        eth_ir_node *expr = build_with_toplvl(bldr, ast->unop.expr, e, false);
        eth_ir_node *ret;
        if (expr->tag == ETH_IR_CVAL)
        {
          ret = constexpr_unop(ast->unop.op, expr->cval.val, e);
          eth_drop_ir_node(expr);
        }
        else
        {
          ret = eth_ir_unop(ast->unop.op, expr);
          eth_set_ir_location(ret, ast->loc);
        }
        return ret;
      }
    }

    case ETH_AST_FN:
      return build_fn(bldr, ast, 0, 0, e);

    // TODO: handle constexprs
    case ETH_AST_MATCH: {
      eth_ir_node *expr = build_with_toplvl(bldr, ast->match.expr, e, false);
      int n1 = bldr->vars->len - bldr->capoffs;
      eth_ir_pattern *pat = build_pattern(bldr, ast->match.pat, ast->loc, e);
      int n2 = bldr->vars->len - bldr->capoffs;
      int nvars = n2 - n1;

      eth_ir_node *thenbr = build_with_toplvl(
          bldr, ast->match.thenbr, e, ast->match.toplvl == ETH_TOPLVL_THEN);

      eth_pop_var(bldr->vars, nvars);

      eth_ast *elseast =
          ast->match.elsebr ? ast->match.elsebr : eth_ast_cval(eth_nil);
      eth_ref_ast(elseast);
      eth_ir_node *elsebr = build_with_toplvl(
          bldr, elseast, e, ast->match.toplvl == ETH_TOPLVL_ELSE);

      eth_ir_node *ret = eth_ir_match(pat, expr, thenbr, elsebr);
      ret->match.toplvl = ast->match.toplvl;
      eth_unref_ast(elseast);
      return ret;
    }

    case ETH_AST_MULTIMATCH: {
      eth_match_table *table = ast->multimatch.table;
      int w = table->w;
      int h = table->h;
      eth_ir_node *exprs[w];
      eth_ir_pattern *ir_table[h][w];
      eth_ir_node *thenbrs[h];

      for (int i = 0; i < w; ++i)
        exprs[i] = build_with_toplvl(bldr, ast->multimatch.exprs[i], e, false);

      for (int i = 0; i < h; ++i)
      {
        int n1 = bldr->vars->len - bldr->capoffs;
        for (int j = 0; j < w; ++j)
          ir_table[i][j] = build_pattern(bldr, table->tab[i][j], ast->loc, e);
        int n2 = bldr->vars->len - bldr->capoffs;
        thenbrs[i] = build_with_toplvl(bldr, table->exprs[i], e, false);
        eth_pop_var(bldr->vars, n2 - n1);
      }

      eth_ir_pattern **tabpats[h];
      for (int i = 0; i < h; ++i)
        tabpats[i] = ir_table[i];

      eth_ir_match_table *tab =
          eth_create_ir_match_table(tabpats, thenbrs, h, w);
      return eth_ir_multimatch(tab, exprs);
    }

    case ETH_AST_AND: {
      if (is_redefined(bldr, "&&"))
      { // user-defined operator -> build as application
        eth_debug("redefined operator '&&'");
        eth_ast *f = eth_ast_ident("&&");
        eth_ast *xs[] = {ast->scand.lhs, ast->scand.rhs};
        eth_ast *apply = eth_ast_apply(f, xs, 2);
        eth_set_ast_location(apply, ast->loc);
        eth_ir_node *ret = build_with_toplvl(bldr, apply, e, false);
        eth_drop_ast(apply);
        return ret;
      }
      else
      {
        eth_ir_node *lhs = build_with_toplvl(bldr, ast->scand.lhs, e, false);
        eth_ir_node *rhs = build_with_toplvl(bldr, ast->scand.rhs, e, false);
        int tmpvar = new_vid(bldr);
        eth_ir_node *ret = eth_ir_bind(
            &tmpvar, &lhs, 1,
            eth_ir_if(eth_ir_var(tmpvar), rhs, eth_ir_cval(eth_false)));
        return ret;
      }
    }

    case ETH_AST_OR: {
      if (is_redefined(bldr, "||"))
      { // user-defined operator -> build as application
        eth_debug("redefined operator '||'");
        eth_ast *f = eth_ast_ident("||");
        eth_ast *xs[] = {ast->scor.lhs, ast->scor.rhs};
        eth_ast *apply = eth_ast_apply(f, xs, 2);
        eth_set_ast_location(apply, ast->loc);
        eth_ir_node *ret = build_with_toplvl(bldr, apply, e, false);
        eth_drop_ast(apply);
        return ret;
      }
      else
      {
        eth_ir_node *lhs = build_with_toplvl(bldr, ast->scor.lhs, e, false);
        eth_ir_node *rhs = build_with_toplvl(bldr, ast->scor.rhs, e, false);
        int tmpvar = new_vid(bldr);
        eth_ir_node *ret =
            eth_ir_bind(&tmpvar, &lhs, 1,
                        eth_ir_if(eth_ir_var(tmpvar), eth_ir_var(tmpvar), rhs));
        return ret;
      }
    }

    /*--------------*
     * Field access *
     *--------------*
     *
     * Build as
     *
     *   MATCH <expr> WTIH { <field> = *tmp* }
     *   THEN *tmp*
     *   ELSE THROW Type_error
     */
    case ETH_AST_ACCESS: {
      eth_ir_node *expr = build_with_toplvl(bldr, ast->access.expr, e, false);
      // --
      if (expr->tag == ETH_IR_CVAL)
      {
        eth_t rec = expr->cval.val;
        eth_t key = eth_sym(ast->access.field);
        if (not eth_is_plain(rec->type))
        { // cant access non-plane type
          eth_warning("undefined field acess (not a plain type: ~w)", rec);
          *e = 1;
          eth_print_location(ast->loc, stderr);
          eth_drop_ir_node(expr);
          return eth_ir_error();
        }
        // perform compile-time access
        int fieldid = eth_get_field_by_id(rec->type, eth_get_symbol_id(key));
        if (fieldid == rec->type->nfields)
        { // field not found
          eth_warning("no field '~d' in ~w", key, rec);
          *e = 1;
          eth_print_location(ast->loc, stderr);
          eth_drop_ir_node(expr);
          return eth_ir_error();
        }
        else
        { // all good
          eth_drop_ir_node(expr);
          return eth_ir_cval(eth_tup_get(rec, fieldid));
        }
      }
      else
      { // run-time access
        int tmpvid = new_vid(bldr);
        eth_ir_pattern *tmpid = eth_ir_ident_pattern(tmpvid);
        size_t field = eth_get_symbol_id(eth_sym(ast->access.field));
        eth_ir_pattern *matchpat = eth_ir_record_pattern(0, &field, &tmpid, 1);
        // if match succeeds return the field value
        eth_ir_node *thenbr = eth_ir_var(tmpvid);
        // else throw type error
        eth_t exn = eth_exn(eth_type_error());
        eth_ir_node *elsebr = eth_ir_throw(eth_ir_cval(exn));
        return eth_ir_match(matchpat, expr, thenbr, elsebr);
      }
    }

    case ETH_AST_TRY: {
      eth_ir_node *trybr = build_with_toplvl(bldr, ast->try.trybr, e, false);

      int exnvid = new_vid(bldr);
      eth_ir_node *exnvar = eth_ir_var(exnvid);

      int n1 = bldr->vars->len - bldr->capoffs;
      eth_ir_pattern *pat = build_pattern(bldr, ast->try.pat, ast->loc, e);
      int n2 = bldr->vars->len - bldr->capoffs;
      int nvars = n2 - n1;
      eth_ir_node *ok = build_with_toplvl(bldr, ast->try.catchbr, e, false);
      eth_pop_var(bldr->vars, nvars);

      eth_ir_node *rethrow = eth_ir_throw(exnvar);

      // don't allow to handle exit-object:
      if (ast->try._check_exit)
      {
        assert(pat->unpack.subpats[0]->tag == ETH_PATTERN_IDENT);
        int what = pat->unpack.subpats[0]->ident.varid;
        int dummy = new_vid(bldr);
        eth_ir_pattern *exitpat =
            eth_ir_unpack_pattern(dummy, eth_exit_type, NULL, NULL, 0);
        ok = eth_ir_match(exitpat, eth_ir_var(what), rethrow, ok);
      }

      eth_ir_node *catchbr = eth_ir_match(pat, exnvar, ok, rethrow);
      return eth_ir_try(exnvid, trybr, catchbr, ast->try.likely);
    }

    case ETH_AST_MKRCRD: {
      eth_type *type = ast->mkrcrd.type;

      if (not eth_is_plain(type))
      {
        eth_error("%s is not a plain type", type->name);
        *e = 1;
        eth_print_location(ast->loc, stderr);
        return eth_ir_error();
      }

      assert(ast->mkrcrd.n == type->nfields);
      int n = type->nfields;
      typedef struct {
        eth_ast *val;
        char *field;
        ptrdiff_t offs;
      } value;
      value vals[n];

      for (int i = 0; i < n; ++i)
      {
        eth_field *fldinfo = eth_get_field(type, ast->mkrcrd.fields[i]);
        assert(fldinfo);
        vals[i].val = ast->mkrcrd.vals[i];
        vals[i].field = ast->mkrcrd.fields[i];
        vals[i].offs = fldinfo->offs;
      }

      // sort fields
      bool err = false;
      int cmp(const void *p1, const void *p2)
      {
        const value *v1 = p1;
        const value *v2 = p2;
        return v1->offs - v2->offs;
      }
      qsort(vals, n, sizeof(value), cmp);

      eth_ir_node *irvals[n];
      for (int i = 0; i < n; ++i)
        irvals[i] = build_with_toplvl(bldr, vals[i].val, e, false);
      return eth_ir_mkrcrd(type, irvals);
    }

    case ETH_AST_UPDATE: {
      eth_ir_node *src = build_with_toplvl(bldr, ast->update.src, e, false);

      int n = ast->update.n;
      typedef struct {
        eth_ast *val;
        char *field;
        size_t id;
      } value;
      value vals[n];

      for (int i = 0; i < n; ++i)
      {
        vals[i].val = ast->update.vals[i];
        vals[i].field = ast->update.fields[i];
        vals[i].id = eth_get_symbol_id(eth_sym(ast->update.fields[i]));
      }

      int cmp(const void *p1, const void *p2)
      {
        const value *v1 = p1;
        const value *v2 = p2;
        return v1->id - v2->id;
      }
      qsort(vals, n, sizeof(value), cmp);

      eth_ir_node *irvals[n];
      size_t ids[n];
      for (int i = 0; i < n; ++i)
      {
        irvals[i] = build_with_toplvl(bldr, vals[i].val, e, false);
        ids[i] = vals[i].id;
      }
      return eth_ir_update(src, irvals, ids, n);
    }

    /*-----------*
     * Assertion *
     *-----------*
     *
     * Build as
     *
     *   IF <expr>
     *   THEN NIL
     *   ELSE THROW Assertion_failed
     *
     * TODO: remove this AST-node; this conversion should be done in parser
     */
    case ETH_AST_ASSERT: {
      eth_use_symbol(assertion_failed);
      eth_ir_node *expr = build_with_toplvl(bldr, ast->assert.expr, e, false);
      eth_ir_node *okbr = eth_ir_cval(eth_nil);
      eth_ir_node *errbr = eth_ir_throw(eth_ir_cval(eth_exn(assertion_failed)));
      eth_ir_node *node = eth_ir_if(expr, okbr, errbr);
      node->iff.likely = 1;
      eth_set_ir_location(okbr, ast->loc);
      eth_set_ir_location(errbr, ast->loc);
      eth_set_ir_location(node, ast->loc);
      return node;
    }

    case ETH_AST_DEFINED: {
      return eth_ir_cval(eth_boolean(find_var_deep(bldr, ast->defined.ident)));
    }

    case ETH_AST_EVMAC: {
      char name[256];
      sprintf(name, "<mac%d>", bldr->immcnt++);

      eth_module *mod = eth_create_module(name, NULL);
      eth_copy_module_path(eth_get_root_env(bldr->root), eth_get_env(mod));
      eth_t ret;
      if (not eth_load_module_from_ast(bldr->root, mod, ast->evmac.expr, &ret))
      {
        eth_error("failed to evaluate macros");
        eth_destroy_module(mod);
        *e = 1;
        eth_print_location(ast->loc, stderr);
        return eth_ir_error();
      }

      if (ret->type == eth_exception_type)
      {
        eth_error("exception thrown during evaluation of macros (~w)", ret);
        eth_drop(ret);
        eth_destroy_module(mod);
        *e = 1;
        eth_print_location(ast->loc, stderr);
        return eth_ir_error();
      }

      eth_destroy_module(mod);

      return eth_ir_cval(ret);
    }

    case ETH_AST_ASSIGN: {
      eth_var var;
      if (not require_var(bldr, ast->ident.str, &var))
      {
        eth_warning("undefined variable, '%s'", ast->ident.str);
        *e = 1;
        eth_print_location(ast->loc, stderr);
        return eth_ir_error();
      }
      if (not(var.attr and var.attr->flag & ETH_ATTR_MUT))
      {
        eth_warning("trying to assign a non-mutable variable");
        *e = 1;
        eth_print_location_opt(ast->loc, stderr, ETH_LOPT_FILE);
        return eth_ir_error();
      }
      if (var.attr && var.attr->flag & ETH_ATTR_DEPRECATED)
      {
        eth_warning("use of deprecated variable, '%s'", ast->ident.str);
        eth_print_location_opt(ast->loc, stderr, ETH_LOPT_FILE);
      }

      eth_t assgn = eth_get_builtin(bldr->root, "__assign");
      eth_ir_node *args[] = {
          eth_ir_var(var.vid),
          build_with_toplvl(bldr, ast->assign.val, e, false),
      };
      eth_ir_node *ret = eth_ir_apply(eth_ir_cval(assgn), args, 2);
      eth_set_ir_location(ret, ast->loc);
      return ret;
    }

    case ETH_AST_RETURN: {
      eth_ir_node *expr = build_with_toplvl(bldr, ast->retrn.expr, e, false);
      eth_ir_node *ret = eth_ir_return(expr);
      eth_set_ir_location(ret, ast->loc);
      return ret;
    }

    case ETH_AST_THIS: {
      eth_ir_node *ret = eth_ir_this();
      eth_set_ir_location(ret, ast->loc);
      return ret;
    }
  }

  eth_error("undefined AST-node");
  *e = 1;
  return eth_ir_error();
}

static eth_ir *
build_ir(eth_ast *ast, eth_root *root, eth_module *mod, eth_ir_defs *defs,
         eth_module *uservars)
{
  eth_ir *ret;
  int e = 0;

  ir_builder *bldr = create_ir_builder(NULL);
  bldr->root = root;
  bldr->istoplvl = true;
  bldr->mod = mod;

  // import builtins:
  import(bldr, eth_get_builtins(root));

  // add extra variables supplied by user
  if (uservars)
    import(bldr, uservars);

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
    defs->defs = bldr->defs.data;
    defs->n = bldr->defs.len;
    bldr->defs.data = NULL;
    bldr->defs.len = 0;
  }

  ret = eth_create_ir(body, bldr->nvars);
  destroy_ir_builder(bldr);
  return ret;
}

eth_ir *
eth_build_ir(eth_ast *ast, eth_root *root, eth_module *uservars)
{
  return build_ir(ast, root, NULL, NULL, uservars);
}

eth_ir *
eth_build_module_ir(eth_ast *ast, eth_root *root, eth_module *mod,
                    eth_ir_defs *defs, eth_module *uservars)
{
  return build_ir(ast, root, mod, defs, uservars);
}

void
eth_destroy_ir_defs(eth_ir_defs *defs)
{
  for (int i = 0; i < defs->n; ++i)
  {
    free(defs->defs[i].ident);
    eth_unref_attr(defs->defs[i].attr);
    if (defs->defs[i].tag == ETH_IRDEF_CVAL)
      eth_unref(defs->defs[i].cval);
  }
  free(defs->defs);
}
