#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>


ETH_MODULE("ether:ir-builder")


typedef struct {
  char *name;
  const eth_module *module;
} module_entry;

typedef struct ir_builder {
  struct ir_builder *parent;
  eth_env *env; // top-level environment
  eth_module *mod; // this module
  bool istoplvl;
  eth_var_list *vars;
  int capoffs;
  int nvars;
  cod_vec(eth_type*) types;
  cod_vec(char*) defidents;
  cod_vec(int  ) defvarids;
  cod_vec(eth_attr*) defattrs;
  cod_vec(module_entry) mods;
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
  bldr->capoffs = 0;
  bldr->nvars = 0;
  cod_vec_init(bldr->types);
  cod_vec_init(bldr->defidents);
  cod_vec_init(bldr->defvarids);
  cod_vec_init(bldr->defattrs);
  cod_vec_init(bldr->mods);

  return bldr;
}

static void
destroy_ir_builder(ir_builder *bldr)
{
  eth_destroy_var_list(bldr->vars);
  cod_vec_destroy(bldr->types);
  cod_vec_iter(bldr->defidents, i, x, free(x));
  cod_vec_destroy(bldr->defidents);
  cod_vec_destroy(bldr->defvarids);
  cod_vec_destroy(bldr->defattrs);
  /*assert(bldr->mods.len == 0);*/
  while (bldr->mods.len > 0)
  { // MODULE will leve dangling modules
    module_entry ent = cod_vec_pop(bldr->mods);
    free(ent.name);
  }
  cod_vec_destroy(bldr->mods);
  free(bldr);
}

static eth_ir_node*
build(ir_builder *bldr, eth_ast *ast, int *e);

static inline void
add_module(ir_builder *bldr, const char *name, const eth_module *mod)
{
  module_entry ent = { .name = strdup(name), .module = mod };
  cod_vec_push(bldr->mods, ent);
}

static inline void
pop_module(ir_builder *bldr)
{
  module_entry ent = cod_vec_pop(bldr->mods);
  free(ent.name);
}

static inline const eth_module*
find_module(ir_builder *bldr, const char *name)
{
  for (size_t i = 0; i < bldr->mods.len; ++i)
  {
    if (strcmp(bldr->mods.data[i].name, name) == 0)
      return bldr->mods.data[i].module;
  }

  if (bldr->parent)
    return find_module(bldr->parent, name);
  else
    return NULL;
}

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
    cod_vec_push(bldr->defidents, strdup(ident));
    cod_vec_push(bldr->defvarids, varid);
    cod_vec_push(bldr->defattrs, attr);
    eth_ref_attr(attr);
  }
  else
  {
    eth_warning("can't have public variables outside top level-scope");
    *e = 1;
    eth_print_location(loc, stderr);
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

  if (bldr->parent)
    return find_type(bldr->parent, name);
  else
    return NULL;
}

static eth_t
resolve_var_path(ir_builder *bldr, const eth_module *mod, const char *path)
{
  char *p;
  if ((p = strchr(path, '.')))
  {
    int modnamelen = p - path;
    char modname[modnamelen + 1];
    memcpy(modname, path, modnamelen);
    modname[modnamelen] = '\0';

    eth_module *submod = eth_require_module(bldr->env, eth_get_env(mod), modname);
    if (submod == NULL)
    {
      eth_warning("no submodule %s in %s", modname, eth_get_module_name(mod));
      return NULL;
    }

    return resolve_var_path(bldr, submod, p + 1);
  }

  eth_def *def = eth_find_def(mod, path);
  if (def == NULL)
  {
    eth_warning("no '%s' in module %s", path, eth_get_module_name(mod));
    return NULL;
  }
  return def->val;
}

const eth_module*
require_module(ir_builder *bldr, const char *name)
{
  assert(bldr->env);

  char *p;
  char topname[256];
  bool istop = true;
  if ((p = strchr(name, '.')))
  {
    int len = p - name;
    memcpy(topname, name, len);
    topname[len] = '\0';
    istop = false;
  }
  else
    strcpy(topname, name);

  // 1. try to load as a submodule
  // 2. else, load with global environment
  // 3. else, load from imported modules
  const eth_module *topmod = NULL, *mod = NULL;
  if (bldr->mod)
  {
    topmod = eth_require_module(bldr->env, eth_get_env(bldr->mod), topname);
    if (not istop)
      mod = eth_require_module(bldr->env, eth_get_env(bldr->mod), name);
  }
  if (not mod)
  {
    topmod = eth_require_module(bldr->env, bldr->env, topname);
    if (not istop)
      mod = eth_require_module(bldr->env, bldr->env, name);
  }
  if (not topmod or not istop and not mod)
  {
    topmod = find_module(bldr, name);
    if (not istop)
      mod = find_module(bldr, name);
  }
  if (not topmod or not istop and not mod)
  {
    eth_warning("failed to resolv module %s", name);
    return NULL;
  }

  if (istop)
    mod = topmod;

  add_module(bldr, topname, topmod);
  return mod;
}

/*
 * Require variable in the scope. Variable will be captured if not found in
 * current scope.
 *
 * Note: Constants wont be captured. If constant variable is returned, it's
 * constant values must be used instead.
 */
static eth_var*
require_var(ir_builder *bldr, const char *ident)
{
  char *p;
  if ((p = strchr(ident, '.')))
  {
    int modnamelen = p - ident;
    char modname[modnamelen + 1];
    memcpy(modname, ident, modnamelen);
    modname[modnamelen] = '\0';

    const eth_module *mod = require_module(bldr, modname);
    if (mod == NULL)
      return NULL;

    eth_t val = resolve_var_path(bldr, mod, p + 1);
    if (val == NULL) return NULL;
    static eth_var ret;
    ret.ident = NULL;
    ret.cval = val;
    ret.vid = -1;
    ret.next = NULL;
    return &ret;
  }
  else
  {
    eth_var *var = eth_find_var(bldr->vars->head, ident, NULL);
    if (var)
    {
      return var;
    }
    else if (bldr->parent)
    {
      if ((var = require_var(bldr->parent, ident)))
      {
        // immediately return constants (don't perform capture)
        if (var->cval) return var;
        int vid = new_vid(bldr);
        bldr->capoffs += 1;
        return  eth_append_var(bldr->vars, eth_copy_var_cfg(var, vid));
      }
    }
    return NULL;
  }
}

eth_var*
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

static bool
import_names(ir_builder *bldr, const eth_module *mod, char *const nams[], int n)
{
  for (int i = 0; i < n; ++i)
  {
    eth_def *def = eth_find_def(mod, nams[i]);
    if (not def)
    {
      eth_warning("no '%s' in module %s", nams[i], eth_get_module_name(mod));
      return false;
    }
    eth_prepend_var(bldr->vars, eth_const_var(def->ident, def->val, def->attr));
  }
  return true;
}

static void
import_default(ir_builder *bldr, const eth_module *mod, const char *alias)
{
  add_module(bldr, alias ? alias : eth_get_module_name(mod), mod);
}

static void
import_unqualified(ir_builder *bldr, const eth_module *mod)
{
  // import vals
  int ndefs = eth_get_ndefs(mod);
  eth_def defs[ndefs];
  eth_get_defs(mod, defs);
  for (int i = 0; i < ndefs; ++i)
  {
    eth_var_cfg varcfg = eth_const_var(defs[i].ident, defs[i].val, defs[i].attr);
    varcfg.attr = defs[i].attr;
    eth_prepend_var(bldr->vars, varcfg);
  }

  // import submodules
  eth_env *modenv = eth_get_env(mod);
  int nsubmods = eth_get_nmodules(modenv);
  const eth_module *submods[nsubmods];
  eth_get_modules(modenv, submods, nsubmods);
  for (int i = 0; i < nsubmods; ++i)
    add_module(bldr, eth_get_module_name(submods[i]), submods[i]);
}

static eth_ir_pattern*
build_pattern(ir_builder *bldr, eth_ast_pattern *pat, eth_location *loc, int *e)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
      return eth_ir_dummy_pattern();

    case ETH_PATTERN_IDENT:
    {
      int varid = new_vid(bldr);

      // set up attribute
      eth_attr *attr = NULL;
      if (pat->ident.attr)
      {
        attr = eth_create_attr(pat->ident.attr);
        // handle PUB
        if (pat->ident.attr & ETH_ATTR_PUB)
          trace_pub_var(bldr, pat->ident.str, varid, attr, loc, e);
      }

      eth_prepend_var(bldr->vars, eth_dyn_var(pat->ident.str, varid, attr));
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
          eth_print_location(loc, stderr);
          return eth_ir_ident_pattern(-1); // just some dummy
        }
      }

      // aliasing:
      int alias = new_vid(bldr);
      if (pat->unpack.alias)
        eth_prepend_var(bldr->vars, eth_dyn_var(pat->unpack.alias, alias, NULL));

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

    case ETH_PATTERN_CONSTANT:
      return eth_ir_constant_pattern(pat->constant.val);

    case ETH_PATTERN_RECORD:
    {
      // aliasing:
      int alias = new_vid(bldr);
      if (pat->record.alias)
        eth_prepend_var(bldr->vars, eth_dyn_var(pat->record.alias, alias, NULL));

      int n = pat->record.n;
      typedef struct { uintptr_t id; eth_ir_pattern *pat; } field;
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
    eth_ir_pattern *pat = build_pattern(bldr, ast->let.pats[idx], ast->loc, e);

    eth_ir_node *thenbr = build_let(bldr, idx + 1, ast, vals, nvars0, e);

    eth_t exn = eth_exn(eth_sym("Type_error"));
    eth_ir_node *elsebr = eth_ir_throw(eth_ir_cval(exn));
    eth_set_ir_location(elsebr, ast->let.vals[idx]->loc);

    eth_ir_node *ret = eth_ir_match(pat, vals[idx], thenbr, elsebr);
    eth_set_ir_location(ret, ast->loc);
    ret->match.toplvl = ETH_TOPLVL_THEN;
    return ret;
  }
  else
  {
    int nvars = bldr->vars->len - bldr->capoffs - nvars0;
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
    eth_set_ir_location(expr, ast->letrec.vals[idx]->loc);

    eth_ir_node *thenbr = build_letrec(bldr, idx + 1, ast, nvars0, nvars, pats, e);

    eth_t exn = eth_exn(eth_sym("Type_error"));
    eth_ir_node *elsebr = eth_ir_throw(eth_ir_cval(exn));
    eth_set_ir_location(elsebr, ast->letrec.vals[idx]->loc);

    eth_ir_node *ret = eth_ir_match(pats[idx], expr, thenbr, elsebr);
    eth_set_ir_location(ret, ast->loc);
    ret->match.toplvl = ETH_TOPLVL_THEN;
    return ret;
  }
  else
  {
    nvars = bldr->vars->len - bldr->capoffs - nvars0;

    int varids[nvars];
    eth_var *it = bldr->vars->head;
    for (int i = nvars-1; i >= 0; --i, it = it->next)
      varids[i] = it->vid;

    eth_ir_node *body = build(bldr, ast->letrec.body, e);
    eth_ir_node *ret = eth_ir_endfix(varids, nvars, body);

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

eth_ir_node*
constexpr_binop(eth_binop op, eth_t lhs, eth_t rhs, eth_location *loc, int *e)
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
        eth_print_location(loc, stderr);
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
        case ETH_LT: ret = eth_boolean(x < y); break;
        case ETH_LE: ret = eth_boolean(x <= y); break;
        case ETH_GT: ret = eth_boolean(x > y); break;
        case ETH_GE: ret = eth_boolean(x >= y); break;
        case ETH_EQ: ret = eth_boolean(x == y); break;
        case ETH_NE: ret = eth_boolean(x != y); break;
        // ---
        default: abort();
      }
      break;
    }

    case ETH_IS: ret = eth_boolean(lhs == rhs); break;
    case ETH_EQUAL: ret = eth_boolean(eth_equal(lhs, rhs)); break;
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
      eth_var *var = require_var(bldr, ast->ident.str);
      if (var == NULL)
      {
        eth_warning("undefined variable, '%s'", ast->ident.str);
        *e = 1;
        eth_print_location(ast->loc, stderr);
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
      eth_ir_node *ret = eth_ir_apply(fn, args, ast->apply.nargs);
      eth_set_ir_location(ret, ast->loc);
      return ret;
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
      return build_let(bldr, 0, ast, vals, bldr->vars->len - bldr->capoffs, e);
    }

    case ETH_AST_LETREC:
    {
      eth_ir_pattern *pats[ast->letrec.n];

      int n1 = bldr->vars->len - bldr->capoffs;
      for (int i = 0; i < ast->letrec.n; ++i)
        pats[i] = build_pattern(bldr, ast->letrec.pats[i], ast->loc, e);
      int n2 = bldr->vars->len - bldr->capoffs;
      int nvars = n2 - n1;

      eth_ir_node *body = build_letrec(bldr, 0, ast, n1, nvars, pats, e);

      int varids[nvars];
      eth_var *it = bldr->vars->head;
      for (int i = nvars-1; i >= 0; --i, it = it->next)
        varids[i] = it->vid;
      eth_pop_var(bldr->vars, nvars);

      return eth_ir_startfix(varids, nvars, body);
    }

    case ETH_AST_BINOP:
    {
      if (is_binop_redefined(bldr, ast->binop.op))
      { // user-defined operator -> build as application
        eth_debug("redefined operator '%s'", eth_binop_sym(ast->binop.op));
        eth_ast *f = eth_ast_ident(eth_binop_sym(ast->binop.op));
        eth_ast *xs[] = { ast->binop.lhs, ast->binop.rhs };
        eth_ast *apply = eth_ast_apply(f, xs, 2);
        eth_set_ast_location(apply, ast->loc);
        eth_ir_node *ret = build(bldr, apply, e);
        eth_drop_ast(apply);
        return ret;
      }
      else
      { // builtin binary operator:
        eth_ir_node *lhs = build(bldr, ast->binop.lhs, e);
        eth_ir_node *rhs = build(bldr, ast->binop.rhs, e);
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

    case ETH_AST_UNOP:
    {
      if (is_unop_redefined(bldr, ast->unop.op))
      { // user-defined operator -> build as application
        eth_debug("redefined operator '%s'", eth_unop_sym(ast->unop.op));
        eth_ast *f = eth_ast_ident(eth_unop_sym(ast->unop.op));
        eth_ast *xs[] = { ast->unop.expr, };
        eth_ast *apply = eth_ast_apply(f, xs, 1);
        eth_set_ast_location(apply, ast->loc);
        eth_ir_node *ret = build(bldr, apply, e);
        eth_drop_ast(apply);
        return ret;
      }
      else
      { // builtin unary operator:
        eth_ir_node *expr = build(bldr, ast->unop.expr, e);
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
        pats[i] = build_pattern(fnbldr, ast->fn.args[i], ast->loc, e);
      int nargvars = fnbldr->vars->len;

      // body:
      eth_ir_node *body_acc = build(fnbldr, ast->fn.body, e);
      eth_ir_node *throw = NULL;
      for (int i = nargs - 1; i >= 0; --i)
      {
        if (throw == NULL)
        {
          eth_t exn = eth_exn(eth_sym("Type_error"));
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
      return eth_ir_fn(ast->fn.arity, caps, capvars, ncap, body, ast);
    }

    case ETH_AST_MATCH:
    {
      eth_ir_node *expr = build(bldr, ast->match.expr, e);
      int n1 = bldr->vars->len - bldr->capoffs;
      eth_ir_pattern *pat = build_pattern(bldr, ast->match.pat, ast->loc, e);
      int n2 = bldr->vars->len - bldr->capoffs;
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

    case ETH_AST_MULTIMATCH:
    {
      eth_match_table *table = ast->multimatch.table;
      int w = table->w;
      int h = table->h;
      eth_ir_node *exprs[w];
      eth_ir_pattern *ir_table[h][w];
      eth_ir_node *thenbrs[h];

      for (int i = 0; i < w; ++i)
        exprs[i] = build(bldr, ast->multimatch.exprs[i], e);

      for (int i = 0; i < h; ++i)
      {
        int n1 = bldr->vars->len - bldr->capoffs;
        for (int j = 0; j < w; ++j)
          ir_table[i][j] = build_pattern(bldr, table->tab[i][j], ast->loc, e);
        int n2 = bldr->vars->len - bldr->capoffs;
        thenbrs[i] = build(bldr, table->exprs[i], e);
        eth_pop_var(bldr->vars, n2 - n1);
      }

      eth_ir_pattern **tabpats[h];
      for (int i = 0; i < h; ++i)
        tabpats[i] = ir_table[i];

      eth_ir_match_table *tab = eth_create_ir_match_table(tabpats, thenbrs, h, w);
      return eth_ir_multimatch(tab, exprs);
    }

    case ETH_AST_IMPORT:
    {
      int n1 = bldr->vars->len - bldr->capoffs;
      int nm1 = bldr->mods.len;
      bool ok = true;

      const eth_module *mod = require_module(bldr, ast->import.module);
      if (not mod)
      {
        *e = 1;
        eth_error("can't import %s", ast->import.module);
        eth_print_location(ast->loc, stderr);
        return eth_ir_error();
      }

      if (ast->import.nams)
        ok = import_names(bldr, mod, ast->import.nams, ast->import.nnam);
      else if (ast->import.alias and ast->import.alias[0] == 0)
        import_unqualified(bldr, mod);
      else
        import_default(bldr, mod, ast->import.alias);

      int nmods = bldr->mods.len - nm1;
      int nvars = bldr->vars->len - bldr->capoffs - n1;
      if (not ok) *e = 1;

      eth_ir_node *body = build(bldr, ast->import.body, e);

      eth_pop_var(bldr->vars, nvars);
      while (nmods--) pop_module(bldr);
      return body;
    }

    case ETH_AST_MODULE:
    {
      // create module
      eth_module *mod = eth_create_module(ast->module.name);
      eth_env *env = bldr->mod ? eth_get_env(bldr->mod) : bldr->env;
      eth_t modret;
      if (not eth_load_module_from_ast(bldr->env, env, mod, ast->module.body, &modret))
      {
        eth_destroy_module(mod);
        *e = 1;
        eth_error("failed to create module");
        eth_print_location(ast->loc, stderr);
        return eth_ir_error();
      }
      // import in current environment
      import_default(bldr, mod, NULL);
      return eth_ir_cval(modret);
    }

    case ETH_AST_AND:
    {
      if (is_redefined(bldr, "&&"))
      { // user-defined operator -> build as application
        eth_debug("redefined operator '&&'");
        eth_ast *f = eth_ast_ident("&&");
        eth_ast *xs[] = { ast->scand.lhs, ast->scand.rhs };
        eth_ast *apply = eth_ast_apply(f, xs, 2);
        eth_set_ast_location(apply, ast->loc);
        eth_ir_node *ret = build(bldr, apply, e);
        eth_drop_ast(apply);
        return ret;
      }
      else
      {
        eth_ir_node *lhs = build(bldr, ast->scand.lhs, e);
        eth_ir_node *rhs = build(bldr, ast->scand.rhs, e);
        int tmpvar = new_vid(bldr);
        eth_ir_node *ret =
          eth_ir_bind(&tmpvar, &lhs, 1,
            eth_ir_if(eth_ir_var(tmpvar), rhs, eth_ir_cval(eth_false)));
        return ret;
      }
    }

    case ETH_AST_OR:
    {
      if (is_redefined(bldr, "||"))
      { // user-defined operator -> build as application
        eth_debug("redefined operator '||'");
        eth_ast *f = eth_ast_ident("||");
        eth_ast *xs[] = { ast->scor.lhs, ast->scor.rhs };
        eth_ast *apply = eth_ast_apply(f, xs, 2);
        eth_set_ast_location(apply, ast->loc);
        eth_ir_node *ret = build(bldr, apply, e);
        eth_drop_ast(apply);
        return ret;
      }
      else
      {
        eth_ir_node *lhs = build(bldr, ast->scor.lhs, e);
        eth_ir_node *rhs = build(bldr, ast->scor.rhs, e);
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
    case ETH_AST_ACCESS:
    {
      eth_use_symbol(Type_error)

      eth_ir_node *expr = build(bldr, ast->access.expr, e);
      // --
      size_t fid = eth_get_symbol_id(eth_sym(ast->access.field));
      int tmpvar = new_vid(bldr);
      eth_ir_pattern *tmp = eth_ir_ident_pattern(tmpvar);
      eth_ir_pattern *pat = eth_ir_record_pattern(new_vid(bldr), &fid, &tmp, 1);
      // --
      eth_ir_node *exn = eth_ir_cval(eth_exn(Type_error));
      // --
      return eth_ir_match(pat, expr, eth_ir_var(tmpvar), eth_ir_throw(exn));
    }

    case ETH_AST_TRY:
    {
      eth_ir_node *trybr = build(bldr, ast->try.trybr, e);

      int exnvid = new_vid(bldr);
      eth_ir_node *exnvar = eth_ir_var(exnvid);

      int n1 = bldr->vars->len - bldr->capoffs;
      eth_ir_pattern *pat = build_pattern(bldr, ast->try.pat, ast->loc, e);
      int n2 = bldr->vars->len - bldr->capoffs;
      int nvars = n2 - n1;
      eth_ir_node *ok = build(bldr, ast->try.catchbr, e);
      eth_pop_var(bldr->vars, nvars);

      eth_ir_node *rethrow = eth_ir_throw(exnvar);

      // don't allow to handle exit-object:
      if (ast->try._check_exit)
      {
        assert(pat->unpack.subpats[0]->tag == ETH_PATTERN_IDENT);
        int what = pat->unpack.subpats[0]->ident.varid;
        int dummy = new_vid(bldr);
        eth_ir_pattern *exitpat = eth_ir_unpack_pattern(dummy, eth_exit_type,
            NULL, NULL, 0);
        ok = eth_ir_match(exitpat, eth_ir_var(what), rethrow, ok);
      }

      eth_ir_node *catchbr = eth_ir_match(pat, exnvar, ok, rethrow);
      return eth_ir_try(exnvid, trybr, catchbr, ast->try.likely);
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
          eth_print_location(ast->loc, stderr);
          return eth_ir_error();
        }
      }
      else
        type = ast->mkrcrd.type.ctype;

      if (not eth_is_plain(type))
      {
        eth_error("%s is not a plain type", type->name);
        *e = 1;
        eth_print_location(ast->loc, stderr);
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
          eth_print_location(ast->loc, stderr);
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
        eth_print_location(ast->loc, stderr);
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
build_ir(eth_ast *ast, eth_env *env, eth_module *mod, eth_ir_defs *defs,
    eth_module *uservars)
{
  eth_ir *ret;
  int e = 0;

  ir_builder *bldr = create_ir_builder(NULL);
  bldr->env = env;
  bldr->istoplvl = true;
  bldr->mod = mod;

  // import builtins:
  import_unqualified(bldr, eth_builtins());

  // add extra variables supplied by user
  if (uservars)
    import_unqualified(bldr, uservars);

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
    defs->attrs = bldr->defattrs.data;
    defs->n = bldr->defidents.len;
    bldr->defidents.data = NULL;
    bldr->defvarids.data = NULL;
    bldr->defattrs.data = NULL;
    cod_vec_destroy(bldr->defidents);
    cod_vec_destroy(bldr->defvarids);
    cod_vec_destroy(bldr->defattrs);
  }

  ret = eth_create_ir(body, bldr->nvars);
  destroy_ir_builder(bldr);
  return ret;
}

eth_ir*
eth_build_ir(eth_ast *ast, eth_env *env, eth_module *uservars)
{
  return build_ir(ast, env, NULL, NULL, uservars);
}

eth_ir*
eth_build_module_ir(eth_ast *ast, eth_env *env, eth_module *mod,
    eth_ir_defs *defs, eth_module *uservars)
{
  return build_ir(ast, env, mod, defs, uservars);
}

void
eth_destroy_ir_defs(eth_ir_defs *defs)
{
  for (int i = 0; i < defs->n; ++i)
  {
    free(defs->idents[i]);
    eth_unref_attr(defs->attrs[i]);
  }
  free(defs->idents);
  free(defs->varids);
  free(defs->attrs);
}

