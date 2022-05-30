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

#include <string.h>
#include <stdlib.h>
#include <assert.h>


ETH_MODULE("ether:ast")


eth_ast_pattern*
eth_ast_dummy_pattern(void)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_AST_PATTERN_DUMMY;
  return pat;
}

eth_ast_pattern*
eth_ast_ident_pattern(const char *ident)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_AST_PATTERN_IDENT;
  pat->ident.str = strdup(ident);
  pat->ident.attr = NULL;
  return pat;
}

void
eth_set_ident_attr(eth_ast_pattern *ident, eth_attr *attr)
{
  eth_ref_attr(attr);
  if (ident->ident.attr)
    eth_unref_attr(ident->ident.attr);
  ident->ident.attr = attr;
}

eth_ast_pattern*
eth_ast_unpack_pattern(eth_type *type, char *const fields[],
    eth_ast_pattern *const pats[], int n)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_AST_PATTERN_UNPACK;
  pat->unpack.type = type;
  pat->unpack.fields = malloc(sizeof(char*) * n);
  pat->unpack.subpats = malloc(sizeof(eth_ast_pattern*) * n);
  pat->unpack.n = n;
  pat->unpack.alias = NULL;
  for (int i = 0; i < n; ++i)
  {
    pat->unpack.fields[i] = strdup(fields[i]);
    eth_ref_ast_pattern(pat->unpack.subpats[i] = pats[i]);
  }
  return pat;
}

eth_ast_pattern*
eth_ast_record_pattern(char *const fields[], eth_ast_pattern *pats[], int n)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_AST_PATTERN_RECORD;
  pat->record.fields = malloc(sizeof(char*) * n);
  pat->record.subpats = malloc(sizeof(eth_ast_pattern*) * n);
  pat->record.n = n;
  pat->record.alias = NULL;
  for (int i = 0; i < n; ++i)
  {
    pat->record.fields[i] = strdup(fields[i]);
    eth_ref_ast_pattern(pat->record.subpats[i] = pats[i]);
  }
  return pat;
}

eth_ast_pattern*
eth_ast_constant_pattern(eth_t val)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_AST_PATTERN_CONSTANT;
  eth_ref(pat->constant.val = val);
  return pat;
}

eth_ast_pattern*
eth_ast_record_star_pattern()
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_AST_PATTERN_RECORD_STAR;
  pat->recordstar.alias = NULL;
  pat->recordstar.attr = NULL;
  return pat;
}

void
eth_set_pattern_alias(eth_ast_pattern *pat, const char *alias)
{
  switch (pat->tag)
  {
    case ETH_AST_PATTERN_UNPACK:
      if (pat->unpack.alias)
        free(pat->unpack.alias);
      pat->unpack.alias = strdup(alias);
      break;

    case ETH_AST_PATTERN_RECORD:
      if (pat->record.alias)
        free(pat->record.alias);
      pat->record.alias = strdup(alias);
      break;

    case ETH_AST_PATTERN_RECORD_STAR:
      if (pat->recordstar.alias)
        free(pat->recordstar.alias);
      pat->recordstar.alias = strdup(alias);
      break;

    default:
      eth_error("aliases are only available for UNPACK and RECORD patterns");
      abort();
  }
}

static void
destroy_ast_pattern(eth_ast_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_AST_PATTERN_DUMMY:
      break;

    case ETH_AST_PATTERN_IDENT:
      if (pat->ident.attr)
        eth_unref_attr(pat->ident.attr);
      free(pat->ident.str);
      break;

    case ETH_AST_PATTERN_UNPACK:
      for (int i = 0; i < pat->unpack.n; ++i)
      {
        free(pat->unpack.fields[i]);
        eth_unref_ast_pattern(pat->unpack.subpats[i]);
      }
      free(pat->unpack.fields);
      free(pat->unpack.subpats);
      if (pat->unpack.alias)
        free(pat->unpack.alias);
      break;

    case ETH_AST_PATTERN_CONSTANT:
      eth_unref(pat->constant.val);
      break;

    case ETH_AST_PATTERN_RECORD:
      for (int i = 0; i < pat->record.n; ++i)
      {
        free(pat->record.fields[i]);
        eth_unref_ast_pattern(pat->record.subpats[i]);
      }
      free(pat->record.fields);
      free(pat->record.subpats);
      if (pat->record.alias)
        free(pat->record.alias);
      break;

    case ETH_AST_PATTERN_RECORD_STAR:
      if (pat->recordstar.attr)
        eth_unref_attr(pat->recordstar.attr);
      if (pat->recordstar.alias)
        free(pat->recordstar.alias);
      break;
  }
  free(pat);
}

void
eth_ref_ast_pattern(eth_ast_pattern *pat)
{
  pat->rc += 1;
}

void
eth_unref_ast_pattern(eth_ast_pattern *pat)
{
  if (--pat->rc == 0)
    destroy_ast_pattern(pat);
}

void
eth_drop_ast_pattern(eth_ast_pattern *pat)
{
  if (pat->rc == 0)
    destroy_ast_pattern(pat);
}

static inline eth_ast*
create_ast_node(eth_ast_tag tag)
{
  eth_ast *node = malloc(sizeof(eth_ast));
  node->rc = 0;
  node->tag = tag;
  node->loc = NULL;
  return node;
}

static inline void
destroy_ast_node(eth_ast *ast)
{
  if (ast == NULL)
    return;

  switch (ast->tag)
  {
    case ETH_AST_CVAL:
      eth_unref(ast->cval.val);
      break;

    case ETH_AST_IDENT:
      free(ast->ident.str);
      break;

    case ETH_AST_APPLY:
      eth_unref_ast(ast->apply.fn);
      for (int i = 0; i < ast->apply.nargs; ++i)
        eth_unref_ast(ast->apply.args[i]);
      free(ast->apply.args);
      break;

    case ETH_AST_IF:
      eth_unref_ast(ast->iff.cond);
      eth_unref_ast(ast->iff.then);
      eth_unref_ast(ast->iff.els);
      break;

    case ETH_AST_SEQ:
      eth_unref_ast(ast->seq.e1);
      eth_unref_ast(ast->seq.e2);
      break;

    case ETH_AST_LET:
    case ETH_AST_LETREC:
      for (int i = 0; i < ast->let.n; ++i)
      {
        eth_unref_ast(ast->let.vals[i]);
        eth_unref_ast_pattern(ast->let.pats[i]);
      }
      free(ast->let.pats);
      free(ast->let.vals);
      eth_unref_ast(ast->let.body);
      break;

    case ETH_AST_BINOP:
      eth_unref_ast(ast->binop.lhs);
      eth_unref_ast(ast->binop.rhs);
      break;

    case ETH_AST_UNOP:
      eth_unref_ast(ast->unop.expr);
      break;

    case ETH_AST_FN:
      for (int i = 0; i < ast->fn.arity; ++i)
        eth_unref_ast_pattern(ast->fn.args[i]);
      free(ast->fn.args);
      eth_unref_ast(ast->fn.body);
      break;

    case ETH_AST_MATCH:
      eth_unref_ast_pattern(ast->match.pat);
      eth_unref_ast(ast->match.expr);
      eth_unref_ast(ast->match.thenbr);
      eth_unref_ast(ast->match.elsebr);
      break;

    case ETH_AST_AND:
    case ETH_AST_OR:
      eth_unref_ast(ast->scor.lhs);
      eth_unref_ast(ast->scor.rhs);
      break;

    case ETH_AST_ACCESS:
      eth_unref_ast(ast->access.expr);
      free(ast->access.field);
      break;

    case ETH_AST_TRY:
      eth_unref_ast_pattern(ast->try.pat);
      eth_unref_ast(ast->try.trybr);
      eth_unref_ast(ast->try.catchbr);
      break;

    case ETH_AST_MKRCRD:
      for (int i = 0; i < ast->mkrcrd.n; ++i)
      {
        free(ast->mkrcrd.fields[i]);
        eth_unref_ast(ast->mkrcrd.vals[i]);
      }
      free(ast->mkrcrd.fields);
      free(ast->mkrcrd.vals);
      break;

    case ETH_AST_UPDATE:
      eth_unref_ast(ast->update.src);
      for (int i = 0; i < ast->update.n; ++i)
      {
        free(ast->update.fields[i]);
        eth_unref_ast(ast->update.vals[i]);
      }
      free(ast->update.fields);
      free(ast->update.vals);
      break;

    case ETH_AST_ASSERT:
      eth_unref_ast(ast->assert.expr);
      break;

    case ETH_AST_DEFINED:
      free(ast->defined.ident);
      break;

    case ETH_AST_EVMAC:
      eth_unref_ast(ast->evmac.expr);
      break;

    case ETH_AST_MULTIMATCH:
      for (int i = 0; i < ast->multimatch.table->h; ++i)
        eth_unref_ast(ast->multimatch.exprs[i]);
      free(ast->multimatch.exprs);
      eth_destroy_match_table(ast->multimatch.table);
      break;

    case ETH_AST_ASSIGN:
      free(ast->assign.ident);
      eth_unref_ast(ast->assign.val);
      break;

    case ETH_AST_RETURN:
      eth_unref_ast(ast->retrn.expr);
      break;

    case ETH_AST_CLASS:
      for (int i = 0; i < ast->clas.npars; ++i)
        eth_unref_ast_pattern(ast->clas.pars[i]);
      for (int i = 0; i < ast->clas.ninherits; ++i)
      {
        free(ast->clas.inherits[i].classname);
        for (int j = 0; j < ast->clas.inherits[i].nargs; ++j)
          eth_unref_ast(ast->clas.inherits[i].args[j]);
        free(ast->clas.inherits[i].args);
      }
      for (int i = 0; i < ast->clas.nvals; ++i)
      {
        free(ast->clas.vals[i].name);
        eth_unref_ast(ast->clas.vals[i].init);
      }
      for (int i = 0; i < ast->clas.nmethods; ++i)
      {
        free(ast->clas.methods[i].name);
        eth_unref_ast(ast->clas.methods[i].fn);
      }
      free(ast->clas.pars);
      free(ast->clas.inherits);
      free(ast->clas.vals);
      free(ast->clas.methods);
  }

  if (ast->loc)
    eth_unref_location(ast->loc);
  free(ast);
}

extern inline void
eth_ref_ast(eth_ast *ast)
{
  ast->rc += 1;
}

extern inline void
eth_unref_ast(eth_ast *ast)
{
  if (--ast->rc == 0)
    destroy_ast_node(ast);
}

extern inline void
eth_drop_ast(eth_ast *ast)
{
  if (ast->rc == 0)
    destroy_ast_node(ast);
}

void
eth_set_ast_location(eth_ast *ast, eth_location *loc)
{
  if (ast->loc)
    eth_unref_location(ast->loc);
  if ((ast->loc = loc))
    eth_ref_location(loc);
}

eth_ast*
eth_ast_cval(eth_t val)
{
  eth_ast *ast = create_ast_node(ETH_AST_CVAL);
  eth_ref(ast->cval.val = val);
  return ast;
}

eth_ast*
eth_ast_ident(const char *ident)
{
  eth_ast *ast = create_ast_node(ETH_AST_IDENT);
  ast->ident.str = strdup(ident);
  return ast;
}

eth_ast*
eth_ast_apply(eth_ast *fn, eth_ast *const *args, int nargs)
{
  eth_ast *ast = create_ast_node(ETH_AST_APPLY);
  eth_ref_ast(ast->apply.fn = fn);
  ast->apply.nargs = nargs;
  ast->apply.args = malloc(sizeof(eth_ast*) * nargs);
  for (int i = 0; i < nargs; ++i)
    eth_ref_ast(ast->apply.args[i] = args[i]);
  return ast;
}

void
eth_ast_append_arg(eth_ast *ast, eth_ast *arg)
{
  assert(ast->tag == ETH_AST_APPLY);
  int nargs = ast->apply.nargs + 1;
  ast->apply.args = realloc(ast->apply.args, sizeof(eth_ast*) * (nargs + 1));
  ast->apply.args[ast->apply.nargs++] = arg;
  eth_ref_ast(arg);
}

eth_ast*
eth_ast_if(eth_ast *cond, eth_ast *then, eth_ast *els)
{
  eth_ast *ast = create_ast_node(ETH_AST_IF);
  eth_ref_ast(ast->iff.cond = cond);
  eth_ref_ast(ast->iff.then = then);
  eth_ref_ast(ast->iff.els  = els );
  return ast;
}

eth_ast*
eth_ast_seq(eth_ast *e1, eth_ast *e2)
{
  eth_ast *ast = create_ast_node(ETH_AST_SEQ);
  eth_ref_ast(ast->seq.e1 = e1);
  eth_ref_ast(ast->seq.e2 = e2);
  return ast;
}

eth_ast*
eth_ast_let(eth_ast_pattern *const pats[], eth_ast *const *vals, int n,
    eth_ast *body)
{
  eth_ast *ast = create_ast_node(ETH_AST_LET);
  eth_ref_ast(ast->let.body = body);
  ast->let.n = n;
  ast->let.pats = malloc(sizeof(eth_ast_pattern*) * n);
  ast->let.vals = malloc(sizeof(eth_ast*) * n);
  for (int i = 0; i < n; ++i)
  {
    eth_ref_ast(ast->let.vals[i] = vals[i]);
    eth_ref_ast_pattern(ast->let.pats[i] = pats[i]);
  }
  return ast;
}

eth_ast*
eth_ast_letrec(eth_ast_pattern *const pats[], eth_ast *const *vals, int n,
    eth_ast *body)
{
  eth_ast *ast = eth_ast_let(pats, vals, n, body);
  ast->tag = ETH_AST_LETREC;
  return ast;
}

eth_ast*
eth_ast_binop(eth_binop op, eth_ast *lhs, eth_ast *rhs)
{
  eth_ast *ast = create_ast_node(ETH_AST_BINOP);
  ast->binop.op = op;
  eth_ref_ast(ast->binop.lhs = lhs);
  eth_ref_ast(ast->binop.rhs = rhs);
  return ast;
}

eth_ast*
eth_ast_unop(eth_unop op, eth_ast *expr)
{
  eth_ast *ast = create_ast_node(ETH_AST_UNOP);
  ast->unop.op = op;
  eth_ref_ast(ast->unop.expr = expr);
  return ast;
}

eth_ast*
eth_ast_fn_with_patterns(eth_ast_pattern *const args[], int arity, eth_ast *body)
{
  eth_ast *ast = create_ast_node(ETH_AST_FN);
  ast->fn.args = malloc(sizeof(eth_ast_pattern*) * arity);
  ast->fn.arity = arity;
  ast->fn.body = body;
  for (int i = 0; i < arity; ++i)
    eth_ref_ast_pattern(ast->fn.args[i] = args[i]);
  eth_ref_ast(body);
  return ast;
}

eth_ast*
eth_ast_fn(char **args, int arity, eth_ast *body)
{
  eth_ast_pattern *pats[arity];
  for (int i = 0; i < arity; ++i)
    pats[i] = eth_ast_ident_pattern(args[i]);
  return eth_ast_fn_with_patterns(pats, arity, body);
}

eth_ast*
eth_ast_match(eth_ast_pattern *pat, eth_ast *expr, eth_ast *thenbr,
    eth_ast *elsebr)
{
  eth_ast *ast = create_ast_node(ETH_AST_MATCH);
  ast->match.pat = pat;
  eth_ref_ast_pattern(pat);
  eth_ref_ast(ast->match.expr = expr);
  eth_ref_ast(ast->match.thenbr = thenbr);
  eth_ref_ast(ast->match.elsebr = elsebr);
  ast->match.toplvl = ETH_TOPLVL_NONE;
  return ast;
}

eth_ast*
eth_ast_and(eth_ast *lhs, eth_ast *rhs)
{
  eth_ast *ast = create_ast_node(ETH_AST_AND);
  eth_ref_ast(ast->scand.lhs = lhs);
  eth_ref_ast(ast->scand.rhs = rhs);
  return ast;
}

eth_ast*
eth_ast_or(eth_ast *lhs, eth_ast *rhs)
{
  eth_ast *ast = create_ast_node(ETH_AST_OR);
  eth_ref_ast(ast->scor.lhs = lhs);
  eth_ref_ast(ast->scor.rhs = rhs);
  return ast;
}

eth_ast*
eth_ast_access(eth_ast *expr, const char *field)
{
  eth_ast *ast = create_ast_node(ETH_AST_ACCESS);
  eth_ref_ast(ast->access.expr = expr);
  ast->access.field = strdup(field);
  return ast;
}

eth_ast*
eth_ast_try(eth_ast_pattern *pat, eth_ast *try, eth_ast *catch, int likely)
{
  // inner pattern must non-dummy because we will need that value
  // to test for exit-object
  if (pat == NULL)
    pat = eth_ast_ident_pattern("");
  else if (pat->tag == ETH_AST_PATTERN_DUMMY)
  {
    eth_drop_ast_pattern(pat);
    pat = eth_ast_ident_pattern("");
  }
  bool check_exit = eth_is_wildcard((int)pat->tag);

  // use exception-pattern so that user is actually matching with `what'
  char *what = "what";
  pat = eth_ast_unpack_pattern(eth_exception_type, &what, &pat, 1);

  eth_ast *ast = create_ast_node(ETH_AST_TRY);
  ast->try.pat = pat;
  eth_ref_ast_pattern(ast->try.pat);
  eth_ref_ast(ast->try.trybr = try);
  eth_ref_ast(ast->try.catchbr = catch);
  ast->try.likely = likely;
  ast->try._check_exit = check_exit;
  return ast;
}

eth_ast*
eth_ast_make_record(eth_type *type, char *const fields[], eth_ast *const vals[],
    int n)
{
  eth_ast *ast = create_ast_node(ETH_AST_MKRCRD);
  ast->mkrcrd.type = type;
  ast->mkrcrd.fields = malloc(sizeof(char*) * n);
  ast->mkrcrd.vals = malloc(sizeof(eth_ast*) * n);
  ast->mkrcrd.n = n;
  for (int i = 0; i < n; ++i)
  {
    eth_ref_ast(ast->mkrcrd.vals[i] = vals[i]);
    ast->mkrcrd.fields[i] = strdup(fields[i]);
  }
  return ast;
}

eth_ast*
eth_ast_update(eth_ast *src, eth_ast *const vals[], char *const fields[], int n)
{
  eth_ast *ast = create_ast_node(ETH_AST_UPDATE);
  eth_ref_ast(ast->update.src = src);
  ast->update.n = n;
  ast->update.fields = malloc(sizeof(char*) * n);
  ast->update.vals = malloc(sizeof(eth_ast*) * n);
  for (int i = 0; i < n; ++i)
  {
    ast->update.fields[i] = strdup(fields[i]);
    eth_ref_ast(ast->update.vals[i] = vals[i]);
  }
  return ast;
}

eth_ast*
eth_ast_assert(eth_ast *expr)
{
  eth_ast *ast = create_ast_node(ETH_AST_ASSERT);
  eth_ref_ast(ast->assert.expr = expr);
  return ast;
}

eth_ast*
eth_ast_defined(const char *ident)
{
  eth_ast *ast = create_ast_node(ETH_AST_DEFINED);
  ast->defined.ident = strdup(ident);
  return ast;
}

eth_ast*
eth_ast_evmac(eth_ast *expr)
{
  eth_ast *ast = create_ast_node(ETH_AST_EVMAC);
  eth_ref_ast(ast->evmac.expr = expr);
  return ast;
}

eth_match_table*
eth_create_match_table(eth_ast_pattern **const tab[], eth_ast *const exprs[],
    int h, int w)
{
  eth_match_table *table = malloc(sizeof(eth_match_table));
  table->tab = malloc(sizeof(eth_ast_pattern*) * h);
  for (int i = 0; i < h; ++i)
  {
    table->tab[i] = malloc(sizeof(eth_ast_pattern*) * w);
    for (int j = 0; j < w; ++j)
      eth_ref_ast_pattern(table->tab[i][j] = tab[i][j]);
  }
  table->exprs = malloc(sizeof(eth_ast*) * h);
  for (int i = 0; i < h; ++i)
    eth_ref_ast(table->exprs[i] = exprs[i]);
  table->w = w;
  table->h = h;
  return table;
}

void
eth_destroy_match_table(eth_match_table *table)
{
  for (int i = 0; i < table->w; ++i)
  {
    for (int j = 0; j < table->h; ++j)
      eth_unref_ast_pattern(table->tab[i][j]);
    free(table->tab[i]);
  }
  free(table->tab);
  for (int j = 0; j < table->h; ++j)
    eth_unref_ast(table->exprs[j]);
  free(table->exprs);
  free(table);
}

eth_ast*
eth_ast_multimatch(eth_match_table *table, eth_ast *const exprs[])
{
  eth_ast *ast = create_ast_node(ETH_AST_MULTIMATCH);
  ast->multimatch.table = table;
  ast->multimatch.exprs = malloc(sizeof(eth_ast*) * table->w);
  for (int i = 0; i < table->w; ++i)
    eth_ref_ast(ast->multimatch.exprs[i] = exprs[i]);
  return ast;
}

eth_ast*
eth_ast_assign(const char *ident, eth_ast *val)
{
  eth_ast *ast = create_ast_node(ETH_AST_ASSIGN);
  ast->assign.ident = strdup(ident);
  eth_ref_ast(ast->assign.val = val);
  return ast;
}

eth_ast*
eth_ast_return(eth_ast *expr)
{
  eth_ast *ast = create_ast_node(ETH_AST_RETURN);
  eth_ref_ast(ast->retrn.expr = expr);
  return ast;
}

eth_ast*
eth_ast_class(eth_ast_pattern *const pars[], int npars,
    eth_class_inherit *inherits, int ninherits, eth_class_val *vals,
    int nvals, eth_class_method *methods, int nmethods)
{
  eth_ast *ast = create_ast_node(ETH_AST_CLASS);
  ast->clas.pars = malloc(sizeof(eth_ast_pattern*) * npars);
  for (int i = 0; i < npars; ++i)
    eth_ref_ast_pattern(ast->clas.pars[i] = pars[i]);
  ast->clas.npars = npars;
  ast->clas.inherits = malloc(sizeof(eth_class_inherit) * ninherits);
  for (int i = 0; i < ninherits; ++i)
  {
    ast->clas.inherits[i] = inherits[i];
    for (int j = 0; j < inherits[i].nargs; ++j)
      eth_ref_ast(ast->clas.inherits[i].args[j]);
  }
  ast->clas.ninherits = ninherits;
  ast->clas.vals = malloc(sizeof(eth_class_val) * nvals);
  memcpy(ast->clas.vals, vals, sizeof(eth_class_val) * nvals);
  ast->clas.nvals = nvals;
  ast->clas.methods = malloc(sizeof(eth_class_method) * nmethods);
  memcpy(ast->clas.methods, methods, sizeof(eth_class_method) * nmethods);
  ast->clas.nmethods = nmethods;
  for (int i = 0; i < nvals; ++i)
    eth_ref_ast(ast->clas.vals[i].init);
  for (int i = 0; i < nmethods; ++i)
    eth_ref_ast(ast->clas.methods[i].fn);
  return ast;
}

static eth_ast_pattern*
ast_to_pattern(eth_ast *ast, bool *e)
{
  switch (ast->tag)
  {
    case ETH_AST_CVAL:
      return eth_ast_constant_pattern(ast->cval.val);

    case ETH_AST_IDENT:
      if (strcmp(ast->ident.str, "_") == 0)
        return eth_ast_dummy_pattern();
      else
        return eth_ast_ident_pattern(ast->ident.str);

    case ETH_AST_BINOP:
      if (ast->binop.op == ETH_CONS)
      {
        char *fields[2] = { "car", "cdr" };
        eth_ast_pattern *pats[2] = {
          ast_to_pattern(ast->binop.lhs, e),
          ast_to_pattern(ast->binop.rhs, e)
        };
        return eth_ast_unpack_pattern(eth_pair_type, fields, pats, 2);
      }
      break;

    case ETH_AST_MKRCRD:
    {
      int n = ast->mkrcrd.n;
      eth_ast_pattern *fldpats[n];
      for (int i = 0; i < n; ++i)
        fldpats[i] = ast_to_pattern(ast->mkrcrd.vals[i], e);
      if (eth_is_tuple(ast->mkrcrd.type))
        return eth_ast_unpack_pattern(ast->mkrcrd.type, ast->mkrcrd.fields, fldpats, n);
      else if (eth_is_record(ast->mkrcrd.type))
        return eth_ast_record_pattern(ast->mkrcrd.fields, fldpats, n);
      else
      {
        eth_error("undexpected type in record-AST");
        abort();
      }
    }

    default:
      break;
  }

  *e = true;
  return eth_ast_dummy_pattern();
}

eth_ast_pattern*
eth_ast_to_pattern(eth_ast *ast)
{
  bool e = false;
  eth_ast_pattern *ret = ast_to_pattern(ast, &e);
  if (e)
  {
    eth_drop_ast_pattern(ret);
    return NULL;
  }
  return ret;
}
