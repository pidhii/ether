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

eth_ir_pattern*
eth_ir_dummy_pattern(void)
{
  eth_ir_pattern *pat = eth_malloc(sizeof(eth_ir_pattern));
  pat->tag = ETH_PATTERN_DUMMY;
  return pat;
}

eth_ir_pattern*
eth_ir_ident_pattern(int varid)
{
  eth_ir_pattern *pat = eth_malloc(sizeof(eth_ir_pattern));
  pat->tag = ETH_PATTERN_IDENT;
  pat->ident.varid = varid;
  return pat;
}

eth_ir_pattern*
eth_ir_unpack_pattern(int varid, eth_type *type, int offs[],
    eth_ir_pattern *pats[], int n)
{
  eth_ir_pattern *pat = eth_malloc(sizeof(eth_ir_pattern));
  pat->tag = ETH_PATTERN_UNPACK;
  pat->unpack.varid = varid;
  pat->unpack.type = type;
  pat->unpack.offs = eth_malloc(sizeof(int) * n);
  pat->unpack.subpats = eth_malloc(sizeof(eth_ir_pattern*) * n);
  pat->unpack.n = n;
  memcpy(pat->unpack.offs, offs, sizeof(int) * n);
  memcpy(pat->unpack.subpats, pats, sizeof(eth_ir_pattern*) * n);
  return pat;
}

eth_ir_pattern*
eth_ir_constant_pattern(eth_t val)
{
  eth_ir_pattern *pat = eth_malloc(sizeof(eth_ir_pattern));
  pat->tag = ETH_PATTERN_CONSTANT;
  eth_ref(pat->constant.val = val);
  return pat;
}

eth_ir_pattern*
eth_ir_record_pattern(int varid, size_t const ids[],
    eth_ir_pattern *const pats[], int n)
{
  eth_ir_pattern *pat = eth_malloc(sizeof(eth_ir_pattern));
  pat->tag = ETH_PATTERN_RECORD;
  pat->record.varid = varid;
  pat->record.ids = eth_malloc(sizeof(size_t) * n);
  pat->record.subpats = eth_malloc(sizeof(eth_ir_pattern*) * n);
  pat->record.n = n;
  memcpy(pat->record.ids, ids, sizeof(size_t) * n);
  memcpy(pat->record.subpats, pats, sizeof(eth_ir_pattern*) * n);
  return pat;
}

void
eth_destroy_ir_pattern(eth_ir_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
      break;

    case ETH_PATTERN_IDENT:
      break;

    case ETH_PATTERN_UNPACK:
      free(pat->unpack.offs);
      for (int i = 0; i < pat->unpack.n; ++i)
        eth_destroy_ir_pattern(pat->unpack.subpats[i]);
      free(pat->unpack.subpats);
      break;

    case ETH_PATTERN_CONSTANT:
      eth_unref(pat->constant.val);
      break;

    case ETH_PATTERN_RECORD:
      free(pat->record.ids);
      for (int i = 0; i < pat->record.n; ++i)
        eth_destroy_ir_pattern(pat->record.subpats[i]);
      free(pat->record.subpats);
      break;
  }
  free(pat);
}


static inline eth_ir_node*
create_ir_node(eth_ir_tag tag)
{
  eth_ir_node *node = eth_malloc(sizeof(eth_ir_node));
  node->rc = 0;
  node->tag = tag;
  node->loc = NULL;
  return node;
}

void
eth_set_ir_location(eth_ir_node *node, eth_location *loc)
{
  if (loc)
  {
    if (node->loc)
      eth_unref_location(node->loc);
    eth_ref_location(node->loc = loc);
  }
}

void
destroy_ir_node(eth_ir_node *node)
{
  switch (node->tag)
  {
    case ETH_IR_ERROR:
      break;

    case ETH_IR_CVAL:
      eth_unref(node->cval.val);
      break;

    case ETH_IR_VAR:
      break;

    case ETH_IR_APPLY:
      eth_unref_ir_node(node->apply.fn);
      for (int i = 0; i < node->apply.nargs; ++i)
        eth_unref_ir_node(node->apply.args[i]);
      free(node->apply.args);
      break;

    case ETH_IR_IF:
      eth_unref_ir_node(node->iff.cond);
      eth_unref_ir_node(node->iff.thenbr);
      eth_unref_ir_node(node->iff.elsebr);
      break;

    case ETH_IR_TRY:
      eth_unref_ir_node(node->try.trybr);
      eth_unref_ir_node(node->try.catchbr);
      break;

    case ETH_IR_SEQ:
      eth_unref_ir_node(node->seq.e1);
      eth_unref_ir_node(node->seq.e2);
      break;

    case ETH_IR_BINOP:
      eth_unref_ir_node(node->binop.lhs);
      eth_unref_ir_node(node->binop.rhs);
      break;

    case ETH_IR_UNOP:
      eth_unref_ir_node(node->unop.expr);
      break;

    case ETH_IR_FN:
      free(node->fn.caps);
      free(node->fn.capvars);
      free(node->fn.scpvars_local);
      eth_unref_ir(node->fn.body);
      eth_unref_ast(node->fn.ast);
      break;

    case ETH_IR_MATCH:
      eth_destroy_ir_pattern(node->match.pat);
      eth_unref_ir_node(node->match.expr);
      eth_unref_ir_node(node->match.thenbr);
      eth_unref_ir_node(node->match.elsebr);
      break;

    case ETH_IR_ACCESS:
      eth_unref_ir_node(node->access.expr);
      if (node->access.alt)
        eth_unref_ir_node(node->access.alt);
      break;

    case ETH_IR_LETREC:
      free(node->letrec.varids);
      for (int i = 0; i < node->letrec.nvars; ++i)
        eth_unref_ir_node(node->letrec.exprs[i]);
      free(node->letrec.exprs);
      eth_unref_ir_node(node->letrec.body);
      break;

    case ETH_IR_MKRCRD:
      for (int i = 0; i < node->mkrcrd.type->nfields; ++i)
        eth_unref_ir_node(node->mkrcrd.fields[i]);
      free(node->mkrcrd.fields);
      break;

    case ETH_IR_THROW:
      eth_unref_ir_node(node->throw.exn);
      break;

    case ETH_IR_RETURN:
      eth_unref_ir_node(node->retrn.expr);
      break;

    case ETH_IR_MULTIMATCH:
      for (int i = 0; i < node->multimatch.table->h; ++i)
        eth_unref_ir_node(node->multimatch.exprs[i]);
      free(node->multimatch.exprs);
      eth_destroy_ir_match_table(node->multimatch.table);
      break;

    case ETH_IR_UPDATE:
      eth_unref_ir_node(node->update.src);
      for (int i = 0; i < node->update.n; ++i)
        eth_unref_ir_node(node->update.fields[i]);
      free(node->update.fields);
      free(node->update.ids);
      break;
  }

  if (node->loc)
    eth_unref_location(node->loc);
  free(node);
}

extern inline void
eth_ref_ir_node(eth_ir_node *node)
{
  node->rc += 1;
}

extern inline void
eth_unref_ir_node(eth_ir_node *node)
{
  if (--node->rc == 0)
    destroy_ir_node(node);
}

extern inline void
eth_drop_ir_node(eth_ir_node *node)
{
  if (node->rc == 0)
    destroy_ir_node(node);
}

eth_ir_node*
eth_ir_error(void)
{
  eth_ir_node *node = create_ir_node(ETH_IR_ERROR);
  return node;
}

eth_ir_node*
eth_ir_cval(eth_t val)
{
  eth_ir_node *node = create_ir_node(ETH_IR_CVAL);
  eth_ref(node->cval.val = val);
  return node;
}

eth_ir_node*
eth_ir_var(int vid)
{
  eth_ir_node *node = create_ir_node(ETH_IR_VAR);
  node->var.vid = vid;
  return node;
}

eth_ir_node*
eth_ir_apply(eth_ir_node *fn, eth_ir_node *const *args, int nargs)
{
  eth_ir_node *node = create_ir_node(ETH_IR_APPLY);
  eth_ref_ir_node(node->apply.fn = fn);
  node->apply.nargs = nargs;
  node->apply.args = eth_malloc(sizeof(eth_ir_node*) * nargs);
  for (int i = 0; i < nargs; ++i)
    eth_ref_ir_node(node->apply.args[i] = args[i]);
  return node;
}

eth_ir_node*
eth_ir_if(eth_ir_node *cond, eth_ir_node *thenbr, eth_ir_node *elsebr)
{
  eth_ir_node *node = create_ir_node(ETH_IR_IF);
  eth_ref_ir_node(node->iff.cond = cond);
  eth_ref_ir_node(node->iff.thenbr = thenbr);
  eth_ref_ir_node(node->iff.elsebr = elsebr);
  node->iff.toplvl = ETH_TOPLVL_NONE;
  node->iff.likely = 0;
  return node;
}

eth_ir_node*
eth_ir_try(int exnvar, eth_ir_node *trybr, eth_ir_node *catchbr, int likely)
{
  eth_ir_node *node = create_ir_node(ETH_IR_TRY);
  node->try.exnvar = exnvar;
  eth_ref_ir_node(node->try.trybr = trybr);
  eth_ref_ir_node(node->try.catchbr = catchbr);
  node->try.likely = likely;
  return node;
}

eth_ir_node*
eth_ir_seq(eth_ir_node *e1, eth_ir_node *e2)
{
  eth_ir_node *node = create_ir_node(ETH_IR_SEQ);
  eth_ref_ir_node(node->seq.e1 = e1);
  eth_ref_ir_node(node->seq.e2 = e2);
  return node;
}

eth_ir_node*
eth_ir_binop(eth_binop op, eth_ir_node *lhs, eth_ir_node *rhs)
{
  eth_ir_node *node = create_ir_node(ETH_IR_BINOP);
  node->binop.op = op;
  eth_ref_ir_node(node->binop.lhs = lhs);
  eth_ref_ir_node(node->binop.rhs = rhs);
  return node;
}

eth_ir_node*
eth_ir_unop(eth_unop op, eth_ir_node *expr)
{
  eth_ir_node *node = create_ir_node(ETH_IR_UNOP);
  node->unop.op = op;
  eth_ref_ir_node(node->unop.expr = expr);
  return node;
}

eth_ir_node*
eth_ir_fn(int arity, int *caps, int *capvars, int ncap, int *scpvars_local,
    int nscpvars, eth_ir *body, eth_ast *ast)
{
  eth_ir_node *node = create_ir_node(ETH_IR_FN);
  node->fn.arity = arity;
  node->fn.caps = eth_malloc(sizeof(int) * ncap);
  memcpy(node->fn.caps, caps, sizeof(int) * ncap);
  node->fn.capvars = eth_malloc(sizeof(int) * ncap);
  memcpy(node->fn.capvars, capvars, sizeof(int) * ncap);
  node->fn.ncap = ncap;
  node->fn.scpvars_local = eth_malloc(sizeof(int) * nscpvars);
  memcpy(node->fn.scpvars_local, scpvars_local, sizeof(int) * nscpvars);
  node->fn.nscpvars = nscpvars;
  eth_ref_ir(node->fn.body = body);
  eth_ref_ast(node->fn.ast = ast);
  return node;
}

eth_ir_node*
eth_ir_match(eth_ir_pattern *pat, eth_ir_node *expr, eth_ir_node *thenbr,
    eth_ir_node *elsebr)
{
  eth_ir_node *node = create_ir_node(ETH_IR_MATCH);
  node->match.pat = pat;
  eth_ref_ir_node(node->match.expr = expr);
  eth_ref_ir_node(node->match.thenbr = thenbr);
  eth_ref_ir_node(node->match.elsebr = elsebr);
  node->match.toplvl = ETH_TOPLVL_NONE;
  node->match.likely = 0;
  return node;
}

eth_ir_node*
eth_ir_access(eth_ir_node *expr, uint64_t fld, eth_ir_node *alt)
{
  eth_ir_node *node = create_ir_node(ETH_IR_ACCESS);
  eth_ref_ir_node(node->access.expr = expr);
  node->access.fld = fld;
  if (alt)
    eth_ref_ir_node(node->access.alt = alt);
  else
    node->access.alt = NULL;
  return node;
}

eth_ir_node*
eth_ir_letrec(int *varids, eth_ir_node **exprs, int nvars, eth_ir_node *body)
{
  eth_ir_node *node = create_ir_node(ETH_IR_LETREC);
  node->letrec.varids = eth_malloc(sizeof(int) * nvars);
  memcpy(node->letrec.varids, varids, sizeof(int) * nvars);
  node->letrec.exprs = eth_malloc(sizeof(eth_ir_node*) * nvars);
  for (int i = 0; i < nvars; ++i)
    eth_ref_ir_node(node->letrec.exprs[i] = exprs[i]);
  node->letrec.nvars = nvars;
  eth_ref_ir_node(node->letrec.body = body);
  return node;
}

static eth_ir_node*
create_bind(int i, int const varids[], eth_ir_node *const vals[], int n,
    eth_ir_node *body)
{
  if (i < n)
  {
    eth_ir_pattern *ident = eth_ir_ident_pattern(varids[i]);
    eth_ir_node *expr = vals[i];
    eth_ir_node *next = create_bind(i + 1, varids, vals, n, body);
    return eth_ir_match(ident, expr, next, eth_ir_error());
  }
  else
    return body;
}

eth_ir_node*
eth_ir_bind(int const varids[], eth_ir_node *const vals[], int n,
    eth_ir_node *body)
{
  return create_bind(0, varids, vals, n, body);
}

eth_ir_node*
eth_ir_mkrcrd(eth_type *type, eth_ir_node *const fields[])
{
  eth_ir_node *node = create_ir_node(ETH_IR_MKRCRD);
  node->mkrcrd.type = type;
  node->mkrcrd.fields = eth_malloc(sizeof(eth_ir_node*) * type->nfields);
  for (int i = 0; i < type->nfields; ++i)
    eth_ref_ir_node(node->mkrcrd.fields[i] = fields[i]);
  return node;
}

eth_ir_node*
eth_ir_update(eth_ir_node *src, eth_ir_node *const fields[], size_t const ids[],
    int n)
{
  eth_ir_node *node = create_ir_node(ETH_IR_UPDATE);
  eth_ref_ir_node(node->update.src = src);
  node->update.n = n;
  node->update.fields = eth_malloc(sizeof(eth_ir_node*) * n);
  node->update.ids = eth_malloc(sizeof(size_t) * n);
  for (int i = 0; i < n; ++i)
  {
    eth_ref_ir_node(node->update.fields[i] = fields[i]);
    node->update.ids[i] = ids[i];
  }
  return node;
}

eth_ir_node*
eth_ir_throw(eth_ir_node *exn)
{
  eth_ir_node *node = create_ir_node(ETH_IR_THROW);
  eth_ref_ir_node(node->throw.exn = exn);
  return node;
}

eth_ir_node*
eth_ir_return(eth_ir_node *expr)
{
  eth_ir_node *node = create_ir_node(ETH_IR_RETURN);
  eth_ref_ir_node(node->retrn.expr = expr);
  return node;
}

eth_ir_match_table*
eth_create_ir_match_table(eth_ir_pattern **const tab[],
    eth_ir_node *const exprs[], int h, int w)
{
  eth_ir_match_table *table = eth_malloc(sizeof(eth_ir_match_table));
  table->tab = eth_malloc(sizeof(eth_ir_pattern*) * h);
  for (int i = 0; i < h; ++i)
  {
    table->tab[i] = eth_malloc(sizeof(eth_ir_pattern*) * w);
    for (int j = 0; j < w; ++j)
      table->tab[i][j] = tab[i][j];
  }
  table->exprs = eth_malloc(sizeof(eth_ir_node*) * h);
  for (int i = 0; i < h; ++i)
    eth_ref_ir_node(table->exprs[i] = exprs[i]);
  table->w = w;
  table->h = h;
  return table;
}

void
eth_destroy_ir_match_table(eth_ir_match_table *table)
{
  for (int i = 0; i < table->w; ++i)
  {
    for (int j = 0; j < table->h; ++j)
      eth_destroy_ir_pattern(table->tab[i][j]);
    free(table->tab[i]);
  }
  free(table->tab);
  for (int j = 0; j < table->h; ++j)
    eth_unref_ir_node(table->exprs[j]);
  free(table->exprs);
  free(table);
}

eth_ir_node*
eth_ir_multimatch(eth_ir_match_table *table, eth_ir_node *const exprs[])
{
  eth_ir_node *node = create_ir_node(ETH_IR_MULTIMATCH);
  node->multimatch.table = table;
  node->multimatch.exprs = eth_malloc(sizeof(eth_ir_node*) * table->w);
  for (int i = 0; i < table->w; ++i)
    eth_ref_ir_node(node->multimatch.exprs[i] = exprs[i]);
  return node;
}

