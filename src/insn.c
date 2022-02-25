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
#include <assert.h>
#include <string.h>

eth_ssa_pattern*
eth_ssa_dummy_pattern(void)
{
  eth_ssa_pattern *pat = malloc(sizeof(eth_ssa_pattern));
  pat->tag = ETH_PATTERN_DUMMY;
  return pat;
}

eth_ssa_pattern*
eth_ssa_ident_pattern(int vid)
{
  eth_ssa_pattern *pat = malloc(sizeof(eth_ssa_pattern));
  pat->tag = ETH_PATTERN_IDENT;
  pat->ident.vid = vid;
  return pat;
}

eth_ssa_pattern*
eth_ssa_constant_pattern(eth_t val, eth_test_op testop, bool dotest)
{
  eth_ssa_pattern *pat = malloc(sizeof(eth_ssa_pattern));
  pat->tag = ETH_PATTERN_CONSTANT;
  eth_ref(pat->constant.val = val);
  pat->constant.testop = testop;
  pat->constant.dotest = dotest;
  return pat;
}

eth_ssa_pattern*
eth_ssa_unpack_pattern(eth_type *type, int const offs[], int const vids[],
    eth_ssa_pattern *const pats[], int n, bool dotest)
{
  eth_ssa_pattern *pat = malloc(sizeof(eth_ssa_pattern));
  pat->tag = ETH_PATTERN_UNPACK;
  pat->unpack.type = type;
  pat->unpack.offs = malloc(sizeof(int) * n);
  pat->unpack.vids = malloc(sizeof(int) * n);
  pat->unpack.subpat = malloc(sizeof(eth_ssa_pattern*) * n);
  pat->unpack.n = n;
  pat->unpack.dotest = dotest;
  memcpy(pat->unpack.offs, offs, sizeof(int) * n);
  memcpy(pat->unpack.vids, vids, sizeof(int) * n);
  memcpy(pat->unpack.subpat, pats, sizeof(eth_ssa_pattern*) * n);
  return pat;
}

eth_ssa_pattern*
eth_ssa_record_pattern(size_t const ids[], int const vids[],
    eth_ssa_pattern *const pats[], int n)
{
  eth_ssa_pattern *pat = malloc(sizeof(eth_ssa_pattern));
  pat->tag = ETH_PATTERN_RECORD;
  pat->record.ids = malloc(sizeof(size_t) * n);
  pat->record.vids = malloc(sizeof(int) * n);
  pat->record.subpat = malloc(sizeof(eth_ssa_pattern*) * n);
  pat->record.n = n;
  pat->record.dotest = true;
  memcpy(pat->record.ids, ids, sizeof(size_t) * n);
  memcpy(pat->record.vids, vids, sizeof(int) * n);
  memcpy(pat->record.subpat, pats, sizeof(eth_ssa_pattern*) * n);
  return pat;
}

void
eth_destroy_ssa_pattern(eth_ssa_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
      break;

    case ETH_PATTERN_IDENT:
      break;

    case ETH_PATTERN_CONSTANT:
      eth_unref(pat->constant.val);
      break;

    case ETH_PATTERN_UNPACK:
      free(pat->unpack.offs);
      free(pat->unpack.vids);
      for (int i = 0; i < pat->unpack.n; ++i)
        eth_destroy_ssa_pattern(pat->unpack.subpat[i]);
      free(pat->unpack.subpat);
      break;

    case ETH_PATTERN_RECORD:
      free(pat->record.ids);
      free(pat->record.vids);
      for (int i = 0; i < pat->record.n; ++i)
        eth_destroy_ssa_pattern(pat->record.subpat[i]);
      free(pat->record.subpat);
      break;
  }
  free(pat);
}

static eth_insn*
create_insn(eth_insn_tag tag)
{
  eth_insn *insn = malloc(sizeof(eth_insn));
  insn->tag = tag;
  insn->out = -1;
  insn->next = NULL;
  insn->prev = NULL;
  insn->flag = 0;
  insn->comment = NULL;
  return insn;
}

void
eth_destroy_insn(eth_insn *insn)
{
  if (insn == NULL)
    return;

  switch (insn->tag)
  {
    case ETH_INSN_CVAL:
      eth_unref(insn->cval.val);
      break;

    case ETH_INSN_APPLY:
    case ETH_INSN_APPLYTC:
      free(insn->apply.args);
      break;

    case ETH_INSN_LOOP:
      free(insn->loop.args);
      break;

    case ETH_INSN_IF:
      eth_destroy_insn_list(insn->iff.thenbr);
      eth_destroy_insn_list(insn->iff.elsebr);
      if (insn->iff.test == ETH_TEST_MATCH)
      {
        eth_destroy_ssa_pattern(insn->iff.pat);
      }
      else if (insn->iff.test == ETH_TEST_UPDATE)
      {
        free(insn->iff.update.vids);
        free(insn->iff.update.ids);
      }
      break;

    case ETH_INSN_FN:
      free(insn->fn.caps);
      eth_unref_ast(insn->fn.ast);
      eth_unref_ir(insn->fn.ir);
      eth_unref_ssa(insn->fn.ssa);
      break;

    case ETH_INSN_FINFN:
      free(insn->finfn.caps);
      free(insn->finfn.movs);
      eth_unref_ast(insn->finfn.ast);
      eth_unref_ir(insn->finfn.ir);
      eth_unref_ssa(insn->finfn.ssa);
      break;

    case ETH_INSN_MKSCP:
      free(insn->mkscp.clos);
      break;

    case ETH_INSN_TRY:
      eth_destroy_insn_list(insn->try.trybr);
      eth_destroy_insn_list(insn->try.catchbr);
      break;

    case ETH_INSN_MKRCRD:
      free(insn->mkrcrd.vids);
      break;

    case ETH_INSN_CAP:
      free(insn->cap.vids);
      break;

    case ETH_INSN_POP:
      free(insn->pop.vids);
      break;

    default:
      break;
  }

  if (insn->comment)
    free(insn->comment);

  free(insn);
}

void
eth_destroy_insn_list(eth_insn *head)
{
  while (head)
  {
    eth_insn *tmp = head->next;
    eth_destroy_insn(head);
    head = tmp;
  }
}

eth_insn*
eth_insn_nop(void)
{
  eth_insn *insn = create_insn(ETH_INSN_NOP);
  return insn;
}

eth_insn*
eth_insn_cval(int out, eth_t val)
{
  eth_insn *insn = create_insn(ETH_INSN_CVAL);
  eth_ref(insn->cval.val = val);
  insn->out = out;
  return insn;
}

static eth_insn*
create_apply(eth_insn_tag tag, int out, int fn, const int *args, int nargs)
{
  eth_insn *insn = create_insn(tag);
  insn->apply.fn = fn;
  insn->apply.nargs = nargs;
  insn->apply.args = malloc(sizeof(int) * nargs);
  memcpy(insn->apply.args, args, sizeof(int) * nargs);
  insn->out = out;
  return insn;
}

eth_insn*
eth_insn_apply(int out, int fn, const int *args, int nargs)
{
  return create_apply(ETH_INSN_APPLY, out, fn, args, nargs);
}

eth_insn*
eth_insn_applytc(int out, int fn, const int *args, int nargs)
{
  return create_apply(ETH_INSN_APPLYTC, out, fn, args, nargs);
}

eth_insn*
eth_insn_loop(const int args[], int nargs)
{
  eth_insn *insn = create_insn(ETH_INSN_LOOP);
  insn->loop.nargs = nargs;
  insn->loop.args = malloc(sizeof(int) * nargs);
  memcpy(insn->loop.args, args, sizeof(int) * nargs);
  return insn;
}

eth_insn*
eth_insn_if(int out, int cond, eth_insn *thenbr, eth_insn *elsebr)
{
  eth_insn *insn = create_insn(ETH_INSN_IF);
  insn->out = out;
  insn->iff.test = ETH_TEST_NOTFALSE;
  insn->iff.cond = cond;
  insn->iff.likely = 0;
  insn->iff.thenbr = thenbr;
  insn->iff.elsebr = elsebr;
  insn->iff.toplvl = ETH_TOPLVL_NONE;
  thenbr->prev = elsebr->prev = insn;
  return insn;
}

eth_insn*
eth_insn_if_test_type(int out, int cond, eth_type *type, eth_insn *thenbr,
    eth_insn *elsebr)
{
  eth_insn *insn = eth_insn_if(out, cond, thenbr, elsebr);
  insn->iff.test = ETH_TEST_TYPE;
  insn->iff.type = type;
  return insn;
}

eth_insn*
eth_insn_if_match(int out, int cond, eth_ssa_pattern *pat, eth_insn *thenbr,
    eth_insn *elsebr)
{
  eth_insn *insn = eth_insn_if(out, cond, thenbr, elsebr);
  insn->iff.test = ETH_TEST_MATCH;
  insn->iff.pat = pat;
  return insn;
}

eth_insn*
eth_insn_if_update(int out, int src, int *vids, size_t *ids, int n,
    eth_insn *thenbr, eth_insn *elsebr)
{
  eth_insn *insn = eth_insn_if(out, src, thenbr, elsebr);
  insn->iff.test = ETH_TEST_UPDATE;
  insn->iff.update.vids = malloc(sizeof(int) * n);
  memcpy(insn->iff.update.vids, vids, sizeof(int) * n);
  insn->iff.update.ids = malloc(sizeof(size_t) * n);
  memcpy(insn->iff.update.ids, ids, sizeof(size_t) * n);
  insn->iff.update.n = n;
  return insn;
}

eth_insn*
eth_insn_mov(int out, int vid)
{
  eth_insn *insn = create_insn(ETH_INSN_MOV);
  insn->out = out;
  insn->var.vid = vid;
  return insn;
}

eth_insn*
eth_insn_ref(int vid)
{
  eth_insn *insn = create_insn(ETH_INSN_REF);
  insn->var.vid = vid;
  return insn;
}

eth_insn*
eth_insn_dec(int vid)
{
  eth_insn *insn = create_insn(ETH_INSN_DEC);
  insn->var.vid = vid;
  return insn;
}

eth_insn*
eth_insn_unref(int vid)
{
  eth_insn *insn = create_insn(ETH_INSN_UNREF);
  insn->var.vid = vid;
  return insn;
}

eth_insn*
eth_insn_drop(int vid)
{
  eth_insn *insn = create_insn(ETH_INSN_DROP);
  insn->var.vid = vid;
  return insn;
}

eth_insn*
eth_insn_ret(int vid)
{
  eth_insn *insn = create_insn(ETH_INSN_RET);
  insn->var.vid = vid;
  return insn;
}

eth_insn*
eth_insn_binop(eth_binop op, int out, int lhs, int rhs)
{
  eth_insn *insn = create_insn(ETH_INSN_BINOP);
  insn->binop.op = op;
  insn->out = out;
  insn->binop.lhs = lhs;
  insn->binop.rhs = rhs;
  return insn;
}

eth_insn*
eth_insn_unop(eth_unop op, int out, int vid)
{
  eth_insn *insn = create_insn(ETH_INSN_UNOP);
  insn->unop.op = op;
  insn->out = out;
  insn->unop.vid = vid;
  return insn;
}

eth_insn*
eth_insn_fn(int out, int arity, int *caps, int ncap, eth_ast *ast, eth_ir *ir,
    eth_ssa *ssa)
{
  eth_insn *insn = create_insn(ETH_INSN_FN);
  insn->out = out;
  insn->fn.arity = arity;
  insn->fn.caps = malloc(sizeof(int) * ncap);
  insn->fn.ncap = ncap;
  insn->fn.ast = ast;
  insn->fn.ir = ir;
  insn->fn.ssa = ssa;
  eth_ref_ast(ast);
  eth_ref_ir(ir);
  eth_ref_ssa(ssa);
  memcpy(insn->fn.caps, caps, sizeof(int) * ncap);
  return insn;
}

eth_insn*
eth_insn_alcfn(int out, int arity)
{
  eth_insn *insn = create_insn(ETH_INSN_ALCFN);
  insn->out = out;
  insn->alcfn.arity = arity;
  return insn;
}

eth_insn*
eth_insn_finfn(int c, int arity, int *caps, int ncap, int *movs, int nmov,
    eth_ast *ast, eth_ir *ir, eth_ssa *ssa)
{
  eth_insn *insn = create_insn(ETH_INSN_FINFN);
  insn->out = c;
  insn->finfn.arity = arity;
  insn->finfn.caps = malloc(sizeof(int) * ncap);
  insn->finfn.ncap = ncap;
  insn->finfn.ast = ast;
  insn->finfn.ir = ir;
  insn->finfn.ssa = ssa;
  eth_ref_ast(ast);
  eth_ref_ir(ir);
  eth_ref_ssa(ssa);
  memcpy(insn->finfn.caps, caps, sizeof(int) * ncap);
  insn->finfn.nmov = nmov;
  insn->finfn.movs = malloc(sizeof(int) * nmov);
  memcpy(insn->finfn.movs, movs, sizeof(int) * nmov);
  return insn;
}

eth_insn*
eth_insn_pop(int const vids[], int n)
{
  eth_insn *insn = create_insn(ETH_INSN_POP);
  insn->pop.vids = malloc(sizeof(int) * n);
  insn->pop.n = n;
  memcpy(insn->pop.vids, vids, sizeof(int) * n);
  return insn;
}

eth_insn*
eth_insn_cap(int const vids[], int n)
{
  eth_insn *insn = create_insn(ETH_INSN_CAP);
  insn->cap.vids = malloc(sizeof(int) * n);
  insn->cap.n = n;
  memcpy(insn->cap.vids, vids, sizeof(int) * n);
  return insn;
}

eth_insn*
eth_insn_mkscp(int *clos, int nclos)
{
  eth_insn *insn = create_insn(ETH_INSN_MKSCP);
  insn->mkscp.clos = malloc(sizeof(int) * nclos);
  insn->mkscp.nclos = nclos;
  memcpy(insn->mkscp.clos, clos, sizeof(int) * nclos);
  return insn;
}

eth_insn*
eth_insn_catch(int tryid, int vid)
{
  eth_insn *insn = create_insn(ETH_INSN_CATCH);
  insn->catch.tryid = tryid;
  insn->catch.vid = vid;
  return insn;
}

eth_insn*
eth_insn_try(int out, int id, eth_insn *trybr, eth_insn *catchbr)
{
  eth_insn *insn = create_insn(ETH_INSN_TRY);
  insn->out = out;
  insn->try.id = id;
  insn->try.likely = 0;
  insn->try.trybr = trybr;
  insn->try.catchbr = catchbr;
  trybr->prev = catchbr->prev = insn;
  return insn;
}

eth_insn*
eth_insn_getexn(int out)
{
  eth_insn *insn = create_insn(ETH_INSN_GETEXN);
  insn->out = out;
  return insn;
}

eth_insn*
eth_insn_mkrcrd(int out, int const vids[], eth_type *type)
{
  eth_insn *insn = create_insn(ETH_INSN_MKRCRD);
  insn->out = out;
  int n = type->nfields;
  insn->mkrcrd.vids = malloc(sizeof(int) * n);
  memcpy(insn->mkrcrd.vids, vids, sizeof(int) * n);
  insn->mkrcrd.type = type;
  return insn;
}

static void
dump_ssa(int ident, const eth_insn *insn, FILE *stream)
{
  if (insn == NULL) return;

  for (int i = 0; i < ident; ++i)
    putc(' ', stream);

  switch (insn->tag)
  {
    case ETH_INSN_NOP:
      fprintf(stream, "nop;\n");
      break;

    case ETH_INSN_CVAL:
      eth_fprintf(stream, "%%%d = cval ~w;\n", insn->out, insn->cval.val);
      break;

    case ETH_INSN_APPLY:
      fprintf(stream, "%%%d = apply %%%d (", insn->out, insn->apply.fn);
      for (int i = 0; i < insn->apply.nargs; ++i)
      {
        if (i > 0) fputs(", ", stream);
        fprintf(stream, "%%%d", insn->apply.args[i]);
      }
      fputs(");\n", stream);
      break;

    case ETH_INSN_APPLYTC:
      fprintf(stream, "%%%d = apply-tc %%%d (", insn->out, insn->apply.fn);
      for (int i = 0; i < insn->apply.nargs; ++i)
      {
        if (i > 0) fputs(", ", stream);
        fprintf(stream, "%%%d", insn->apply.args[i]);
      }
      fputs(");\n", stream);
      break;

    case ETH_INSN_LOOP:
      fprintf(stream, "loop (");
      for (int i = 0; i < insn->loop.nargs; ++i)
      {
        if (i > 0) fputs(", ", stream);
        fprintf(stream, "%%%d", insn->loop.args[i]);
      }
      fputs(");\n", stream);
      break;

    case ETH_INSN_IF:
      fprintf(stream, "if ");
      if (insn->out >= 0) fprintf(stream, "<phi %%%d> ", insn->out);
      if (insn->iff.likely > 0) fprintf(stream, "<likely> ");
      else if (insn->iff.likely < 0) fprintf(stream, "<unlikely> ");
      switch (insn->iff.test)
      {
        case ETH_TEST_NOTFALSE:
          fprintf(stream, "<not-false> ");
          break;
        case ETH_TEST_TYPE:
          fprintf(stream, "<type %s> ", insn->iff.type->name);
          break;
        case ETH_TEST_MATCH:
          fprintf(stream, "<match ...> ");
          break;
        case ETH_TEST_UPDATE:
          fprintf(stream, "<update ...> ");
          break;
      }
      fprintf(stream, "%%%d then\n", insn->iff.cond);

      dump_ssa(ident + 2, insn->iff.thenbr, stream);

      for (int i = 0; i < ident; ++i) putc(' ', stream);
      fputs("else\n", stream);

      dump_ssa(ident + 2, insn->iff.elsebr, stream);

      for (int i = 0; i < ident; ++i) putc(' ', stream);
      fputs("end\n", stream);
      break;

    case ETH_INSN_MOV:
      fprintf(stream, "%%%d = mov %%%d;\n", insn->out, insn->var.vid);
      break;

    case ETH_INSN_REF:
      fprintf(stream, "ref %%%d;", insn->var.vid);
      if (insn->comment)
        fprintf(stream, " # %s\n", insn->comment);
      else
        fprintf(stream, "\n");
      break;

    case ETH_INSN_DEC:
      fprintf(stream, "dec %%%d;\n", insn->var.vid);
      break;

    case ETH_INSN_UNREF:
      fprintf(stream, "unref %%%d;\n", insn->var.vid);
      break;

    case ETH_INSN_DROP:
      fprintf(stream, "drop %%%d;\n", insn->var.vid);
      break;

    case ETH_INSN_RET:
      fprintf(stream, "ret %%%d;\n", insn->var.vid);
      break;

    case ETH_INSN_BINOP:
      fprintf(stream, "%%%d = %s %%%d %%%d;\n", insn->out,
          eth_binop_name(insn->binop.op), insn->binop.lhs, insn->binop.rhs);
      break;

    case ETH_INSN_UNOP:
      fprintf(stream, "%%%d = %s %%%d;\n", insn->out,
          eth_unop_name(insn->unop.op), insn->unop.vid);
      break;

    case ETH_INSN_FN:
      fprintf(stream, "%%%d = fn/%d [", insn->out, insn->fn.arity);
      for (int i = 0; i < insn->fn.ncap; ++i)
      {
        if (i > 0) fputs(", ", stream);
        fprintf(stream, "%%%d", insn->fn.caps[i]);
      }
      fputs("]\n", stream);
      dump_ssa(ident + 2, insn->fn.ssa->body, stream);
      for (int i = 0; i < ident; ++i)
        putc(' ', stream);
      fputs("end\n", stream);
      break;

    case ETH_INSN_ALCFN:
      fprintf(stream, "%%%d = alcfn/%d;\n", insn->out, insn->alcfn.arity);
      break;

    case ETH_INSN_FINFN:
      fprintf(stream, "finfn/%d <%%%d> [", insn->finfn.arity, insn->out);
      for (int i = 0; i < insn->finfn.ncap; ++i)
      {
        if (i > 0) fputs(", ", stream);
        fprintf(stream, "%%%d", insn->finfn.caps[i]);
      }
      fputs("]\n", stream);
      dump_ssa(ident + 2, insn->finfn.ssa->body, stream);
      for (int i = 0; i < ident; ++i)
        putc(' ', stream);
      fputs("end\n", stream);
      break;

    case ETH_INSN_POP:
      for (int i = 0; i < insn->pop.n; ++i)
      {
        if (i > 0) fputs(", ", stream);
        fprintf(stream, "%%%d", insn->pop.vids[i]);
      }
      fprintf(stream, " = pop;\n");
      break;

    case ETH_INSN_CAP:
      for (int i = 0; i < insn->cap.n; ++i)
      {
        if (i > 0) fputs(", ", stream);
        fprintf(stream, "%%%d", insn->cap.vids[i]);
      }
      fprintf(stream, " = cap;\n");
      break;

    case ETH_INSN_MKSCP:
      fputs("mkscp <wref: ", stream);
      fputs("; clos: ", stream);
      for (int i = 0; i < insn->mkscp.nclos; ++i)
        fprintf(stream, i > 0 ? " %%%d" : "%%%d", insn->mkscp.clos[i]);
      fputs(">;\n", stream);
      break;

    case ETH_INSN_CATCH:
      fprintf(stream, "catch %%%d;\n", insn->catch.vid);
      break;

    case ETH_INSN_TRY:
      fprintf(stream, "try ");
      if (insn->out >= 0) fprintf(stream, "<phi %%%d> ", insn->out);
      if (insn->try.likely > 0) fprintf(stream, "<likely> ");
      else if (insn->try.likely < 0) fprintf(stream, "<unlikely> ");
      fprintf(stream, "\n");

      dump_ssa(ident + 2, insn->try.trybr, stream);

      for (int i = 0; i < ident; ++i) putc(' ', stream);
      fputs("except\n", stream);

      dump_ssa(ident + 2, insn->try.catchbr, stream);

      for (int i = 0; i < ident; ++i) putc(' ', stream);
      fputs("end\n", stream);
      break;

    case ETH_INSN_GETEXN:
      fprintf(stream, "%%%d = getexn;\n", insn->out);
      break;

    case ETH_INSN_MKRCRD:
      fprintf(stream, "%%%d = mkrcrd", insn->out);
      for (int i = 0; i < insn->mkrcrd.type->nfields; ++i)
        fprintf(stream, " %%%d", insn->mkrcrd.vids[i]);
      fputs(";\n", stream);
      break;
  }

  dump_ssa(ident, insn->next, stream);
}

void
eth_dump_ssa(const eth_insn *head, FILE *stream)
{
  dump_ssa(0, head, stream);
}

void
eth_set_insn_comment(eth_insn *insn, const char *comment)
{
  if (insn->comment)
    free(insn->comment);
  insn->comment = strdup(comment);
}

