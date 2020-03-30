#include "ether/ether.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

eth_ssa_pattern*
eth_ssa_ident_pattern(int vid)
{
  eth_ssa_pattern *pat = malloc(sizeof(eth_ssa_pattern));
  pat->tag = ETH_PATTERN_IDENT;
  pat->ident.vid = vid;
  return pat;
}

eth_ssa_pattern*
eth_ssa_unpack_pattern(eth_type *type, int const offs[], int const vids[],
    eth_ssa_pattern *const pats[], int n)
{
  eth_ssa_pattern *pat = malloc(sizeof(eth_ssa_pattern));
  pat->tag = ETH_PATTERN_UNPACK;
  pat->unpack.type = type;
  pat->unpack.offs = malloc(sizeof(int) * n);
  pat->unpack.vids = malloc(sizeof(int) * n);
  pat->unpack.subpat = malloc(sizeof(eth_ssa_pattern*) * n);
  pat->unpack.n = n;
  memcpy(pat->unpack.offs, offs, sizeof(int) * n);
  memcpy(pat->unpack.vids, vids, sizeof(int) * n);
  memcpy(pat->unpack.subpat, pats, sizeof(eth_ssa_pattern*) * n);
  return pat;
}

void
eth_destroy_ssa_pattern(eth_ssa_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_IDENT:
      break;

    case ETH_PATTERN_UNPACK:
      free(pat->unpack.offs);
      free(pat->unpack.vids);
      for (int i = 0; i < pat->unpack.n; ++i)
        eth_destroy_ssa_pattern(pat->unpack.subpat[i]);
      free(pat->unpack.subpat);
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

    case ETH_INSN_IF:
      eth_destroy_insn_list(insn->iff.thenbr);
      eth_destroy_insn_list(insn->iff.elsebr);
      if (insn->iff.test == ETH_TEST_MATCH)
        eth_destroy_ssa_pattern(insn->iff.pat);
      break;

    case ETH_INSN_FN:
      free(insn->fn.caps);
      eth_unref_ir(insn->fn.ir);
      eth_destroy_ssa(insn->fn.ssa);
      break;

    case ETH_INSN_FINFN:
      free(insn->finfn.caps);
      free(insn->finfn.movs);
      eth_unref_ir(insn->finfn.ir);
      eth_destroy_ssa(insn->finfn.ssa);
      break;

    case ETH_INSN_MKSCP:
      free(insn->mkscp.clos);
      free(insn->mkscp.wrefs);
      break;

    default:
      break;
  }

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
eth_insn_fn(int out, int arity, int *caps, int ncap, eth_ir *ir, eth_ssa *ssa)
{
  eth_insn *insn = create_insn(ETH_INSN_FN);
  insn->out = out;
  insn->fn.arity = arity;
  insn->fn.caps = malloc(sizeof(int) * ncap);
  insn->fn.ncap = ncap;
  insn->fn.ir = ir;
  insn->fn.ssa = ssa;
  eth_ref_ir(ir);
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
    eth_ir *ir, eth_ssa *ssa)
{
  eth_insn *insn = create_insn(ETH_INSN_FINFN);
  insn->out = c;
  insn->finfn.arity = arity;
  insn->finfn.caps = malloc(sizeof(int) * ncap);
  insn->finfn.ncap = ncap;
  insn->finfn.ir = ir;
  insn->finfn.ssa = ssa;
  eth_ref_ir(ir);
  memcpy(insn->finfn.caps, caps, sizeof(int) * ncap);
  insn->finfn.nmov = nmov;
  insn->finfn.movs = malloc(sizeof(int) * nmov);
  memcpy(insn->finfn.movs, movs, sizeof(int) * nmov);
  return insn;
}

eth_insn*
eth_insn_pop(int vid0, int n)
{
  eth_insn *insn = create_insn(ETH_INSN_POP);
  insn->out = vid0;
  insn->pop.n = n;
  return insn;
}

eth_insn*
eth_insn_cap(int vid0, int n)
{
  eth_insn *insn = create_insn(ETH_INSN_CAP);
  insn->out = vid0;
  insn->cap.n = n;
  return insn;
}

eth_insn*
eth_insn_mkscp(int *clos, int nclos, int *wrefs, int nwref)
{
  eth_insn *insn = create_insn(ETH_INSN_MKSCP);
  insn->mkscp.clos = malloc(sizeof(int) * nclos);
  insn->mkscp.nclos = nclos;
  insn->mkscp.wrefs = malloc(sizeof(int) * nwref);
  insn->mkscp.nwref = nwref;
  memcpy(insn->mkscp.clos, clos, sizeof(int) * nclos);
  memcpy(insn->mkscp.wrefs, wrefs, sizeof(int) * nwref);
  return insn;
}

void
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
      fprintf(stream, "ref %%%d;\n", insn->var.vid);
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
      if (insn->pop.n == 1)
      {
        fprintf(stream, "%%%d = pop;\n", insn->out);
      }
      else
      {
        for (int i = 0; i < insn->pop.n; ++i)
        {
          if (i > 0) fputs(", ", stream);
          fprintf(stream, "%%%d", insn->out + i);
        }
        fprintf(stream, " = pop;\n");
      }
      break;

    case ETH_INSN_CAP:
      if (insn->cap.n == 1)
      {
        fprintf(stream, "%%%d = cap;\n", insn->out);
      }
      else
      {
        for (int i = 0; i < insn->cap.n; ++i)
        {
          if (i > 0) fputs(", ", stream);
          fprintf(stream, "%%%d", insn->out + i);
        }
        fprintf(stream, " = cap;\n");
      }
      break;

    case ETH_INSN_MKSCP:
      fputs("mkscp <wref: ", stream);
      for (int i = 0; i < insn->mkscp.nwref; ++i)
        fprintf(stream, i > 0 ? " %%%d" : "%%%d", insn->mkscp.wrefs[i]);
      fputs("; clos: ", stream);
      for (int i = 0; i < insn->mkscp.nclos; ++i)
        fprintf(stream, i > 0 ? " %%%d" : "%%%d", insn->mkscp.clos[i]);
      fputs(">;\n", stream);
      break;
  }

  dump_ssa(ident, insn->next, stream);
}

void
eth_dump_ssa(const eth_insn *head, FILE *stream)
{
  dump_ssa(0, head, stream);
}
