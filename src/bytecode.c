#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

static void
destroy_insn(eth_bc_insn *insn)
{
  switch (insn->opc)
  {
    case ETH_OPC_CVAL:
      eth_unref(insn->cval.val);
      break;

    case ETH_OPC_PUSH:
      free(insn->push.vids);
      break;

    case ETH_OPC_FN:
    case ETH_OPC_FINFN:
      eth_unref_ir(insn->fn.data->ir);
      eth_unref_bytecode(insn->fn.data->bc);
      free(insn->fn.data);
      break;

    case ETH_OPC_MKSCP:
      free(insn->mkscp.clos);
      free(insn->mkscp.wrefs);
      break;

    default:
      break;
  }
}

typedef struct {
  int jmppos; /* Insn jumping to this block (to be set up after insertion). */
  int retpos; /* Point to return after the block. */
  eth_insn *code;
} deff_block;

typedef struct {
  int len, cap; /* Length and capacity of code-tape. */
  eth_bc_insn *arr; /* Code(output)-tape. */
  cod_vec(deff_block) deff; /* Deffered blocks. */
} builder;

static builder*
create_builder(void)
{
  builder *bldr = malloc(sizeof(builder));
  bldr->len = 0;
  bldr->cap = 0x40;
  bldr->arr = malloc(sizeof(eth_bc_insn) * bldr->cap);
  cod_vec_init(bldr->deff);
  return bldr;
}

static void
destroy_builder(builder *bldr)
{
  cod_vec_destroy(bldr->deff);
  free(bldr);
}

static eth_bc_insn*
append_insn(builder *bldr)
{
  if (eth_unlikely(bldr->cap == bldr->len))
  {
    bldr->cap <<= 1;
    bldr->arr = realloc(bldr->arr, sizeof(eth_bc_insn) * bldr->cap);
  }
  return bldr->arr + bldr->len++;
}

static int
write_cval(builder *bldr, int out, eth_t val)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_CVAL;
  insn->cval.out = out;
  eth_ref(insn->cval.val = val);
  return bldr->len - 1;
}

static int
write_push(builder *bldr, int *vids, int n)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_PUSH;
  insn->push.vids = malloc(sizeof(size_t) * n);
  for (int i = 0; i < n; ++i)
    insn->push.vids[i] = vids[i];
  insn->push.n = n;
  return bldr->len - 1;
}

static int
write_pop(builder *bldr, int vid0, int n)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_POP;
  insn->pop.vid0 = vid0;
  insn->pop.n = n;
  return bldr->len - 1;
}

static int
write_apply(builder *bldr, int out, int fn)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_APPLY;
  insn->apply.out = out;
  insn->apply.fn = fn;
  return bldr->len - 1;
}

static int
write_applytc(builder *bldr, int out, int fn)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_APPLYTC;
  insn->apply.out = out;
  insn->apply.fn = fn;
  return bldr->len - 1;
}

static int
write_test(builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_TEST;
  insn->test.vid = vid;
  return bldr->len - 1;
}

static int
write_testty(builder *bldr, int vid, eth_type *type)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_TESTTY;
  insn->testty.vid = vid;
  insn->testty.type = type;
  return bldr->len - 1;
}

static int
write_gettest(builder *bldr, int out)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_GETTEST;
  insn->gettest.out = out;
  return bldr->len - 1;
}

static int
write_dup(builder *bldr, int out, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_DUP;
  insn->dup.out = out;
  insn->dup.vid = vid;
  return bldr->len - 1;
}

static int
write_jnz(builder *bldr, ptrdiff_t offs)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_JNZ;
  insn->jnz.offs = offs;
  return bldr->len - 1;
}

static int
write_jze(builder *bldr, ptrdiff_t offs)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_JZE;
  insn->jze.offs = offs;
  return bldr->len - 1;
}

static int
write_jmp(builder *bldr, ptrdiff_t offs)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_JMP;
  insn->jmp.offs = offs;
  return bldr->len - 1;
}

static int
write_ref(builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_REF;
  insn->ref.vid = vid;
  return bldr->len - 1;
}

static int
write_dec(builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_DEC;
  insn->dec.vid = vid;
  return bldr->len - 1;
}

static int
write_unref(builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_UNREF;
  insn->unref.vid = vid;
  return bldr->len - 1;
}

static int
write_drop(builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_DROP;
  insn->drop.vid = vid;
  return bldr->len - 1;
}

static int
write_ret(builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_RET;
  insn->ret.vid = vid;
  return bldr->len - 1;
}

static int
write_fn(builder *bldr, int out, int arity, eth_ir *ir, eth_bytecode *bc,
    int *caps, int ncap)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_FN;
  insn->fn.out = out;
  insn->fn.data = malloc(sizeof(insn->fn.data[0]) + sizeof(int) * ncap);
  insn->fn.data->arity = arity;
  insn->fn.data->ir = ir;
  insn->fn.data->bc = bc;
  insn->fn.data->ncap = ncap;
  eth_ref_ir(ir);
  eth_ref_bytecode(bc);
  memcpy(insn->fn.data->caps, caps, sizeof(int) * ncap);
  return bldr->len - 1;
}

static int
write_alcfn(builder *bldr, int out, int arity)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_ALCFN;
  insn->alcfn.out = out;
  insn->alcfn.arity = arity;
  return bldr->len - 1;
}

static int
write_cap(builder *bldr, int vid0, int n)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_CAP;
  insn->cap.vid0 = vid0;
  insn->cap.n = n;
  return bldr->len - 1;
}

static int
write_finfn(builder *bldr, int out, int arity, eth_ir *ir, eth_bytecode *bc,
    int *caps, int ncap)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_FINFN;
  insn->fn.out = out;
  insn->fn.data = malloc(sizeof(insn->fn.data[0]) + sizeof(int) * ncap);
  insn->fn.data->arity = arity;
  insn->fn.data->ir = ir;
  insn->fn.data->bc = bc;
  insn->fn.data->ncap = ncap;
  eth_ref_ir(ir);
  eth_ref_bytecode(bc);
  memcpy(insn->fn.data->caps, caps, sizeof(int) * ncap);
  return bldr->len - 1;
}

static int
write_mkscp(builder *bldr, int *clos, int nclos, int *wrefs, int nwref)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_MKSCP;
  insn->mkscp.clos = malloc(sizeof(size_t) * nclos);
  insn->mkscp.nclos = nclos;
  insn->mkscp.wrefs = malloc(sizeof(size_t) * nwref);
  insn->mkscp.nwref = nwref;
  for (int i = 0; i < nclos; ++i)
    insn->mkscp.clos[i] = clos[i];
  for (int i = 0; i < nwref; ++i)
    insn->mkscp.wrefs[i] = wrefs[i];
  return bldr->len - 1;
}

static int
write_load(builder *bldr, int out, int vid, int offs)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_LOAD;
  insn->load.out = out;
  insn->load.vid = vid;
  insn->load.offs = offs;
  return bldr->len - 1;
}

#define MAKE_BINOP(name, op)                             \
  static int                                             \
  write_##name(builder *bldr, int out, int lhs, int rhs) \
  {                                                      \
    eth_bc_insn *insn = append_insn(bldr);               \
    insn->opc = ETH_OPC_##op;                            \
    insn->binop.out = out;                               \
    insn->binop.lhs = lhs;                               \
    insn->binop.rhs = rhs;                               \
    return bldr->len - 1;                                \
  }

MAKE_BINOP(add, ADD)
MAKE_BINOP(sub, SUB)
MAKE_BINOP(mul, MUL)
MAKE_BINOP(div, DIV)
MAKE_BINOP(lt, LT)
MAKE_BINOP(le, LE)
MAKE_BINOP(gt, GT)
MAKE_BINOP(ge, GE)
MAKE_BINOP(eq, EQ)
MAKE_BINOP(ne, NE)
MAKE_BINOP(is, IS)
MAKE_BINOP(cons, CONS)

typedef cod_vec(int) int_vec;

// TODO: optimize (first unpacks, then idents)
static void
build_pattern(builder *bldr, eth_ssa_pattern *pat, int expr, int_vec *jmps)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_IDENT:
      break;

    case ETH_PATTERN_UNPACK:
    {
      if (pat->unpack.type)
      {
        write_testty(bldr, expr, pat->unpack.type);
        int jmp = write_jze(bldr, -1);
        cod_vec_push(*jmps, jmp);
      }

      for (int i = 0; i < pat->unpack.n; ++i)
      {
        write_load(bldr, pat->unpack.vids[i], expr, pat->unpack.offs[i]);
        build_pattern(bldr, pat->unpack.subpat[i], pat->unpack.vids[i], jmps);
      }

      break;
    }
  }
}

static void
build(builder *bldr, eth_insn *ssa)
{
  for (eth_insn *ip = ssa; ip; ip = ip->next)
  {
    switch (ip->tag)
    {
      case ETH_INSN_NOP:
        break;

      case ETH_INSN_CVAL:
        write_cval(bldr, ip->out, ip->cval.val);
        break;

      case ETH_INSN_APPLY:
        write_push(bldr, ip->apply.args, ip->apply.nargs);
        write_apply(bldr, ip->out, ip->apply.fn);
        break;

      case ETH_INSN_APPLYTC:
        write_push(bldr, ip->apply.args, ip->apply.nargs);
        write_applytc(bldr, ip->out, ip->apply.fn);
        break;

      case ETH_INSN_IF:
      {
        // test:
        int_vec jmps;
        cod_vec_init(jmps);
        switch (ip->iff.test)
        {
          case ETH_TEST_NOTFALSE:
            write_test(bldr, ip->iff.cond);
            goto if_notfalse;

          case ETH_TEST_TYPE:
            write_testty(bldr, ip->iff.cond, ip->iff.type);
            goto if_type;

          case ETH_TEST_MATCH:
            build_pattern(bldr, ip->iff.pat, ip->iff.cond, &jmps);
            goto if_match;
        }

if_notfalse:
if_type:
        // branches:
        if (ip->iff.likely > 0)
        {
          int jmpidx = write_jze(bldr, -1);
          build(bldr, ip->iff.thenbr);
          cod_vec_push(bldr->deff, ((deff_block) {
            .jmppos = jmpidx,
            .retpos = bldr->len,
            .code = ip->iff.elsebr,
          }));
        }
        else if (ip->iff.likely < 0)
        {
          int jmpidx = write_jnz(bldr, -1);
          build(bldr, ip->iff.elsebr);
          cod_vec_push(bldr->deff, ((deff_block) {
            .jmppos = jmpidx,
            .retpos = bldr->len,
            .code = ip->iff.thenbr,
          }));
        }
        else
        {
          int jmpidx = write_jze(bldr, -1);
          build(bldr, ip->iff.thenbr);
          int sepidx = write_jmp(bldr, -1);
          build(bldr, ip->iff.elsebr);
          bldr->arr[jmpidx].jze.offs = sepidx - jmpidx + 1;
          bldr->arr[sepidx].jmp.offs = bldr->len - sepidx;
        }
        goto end_if;

if_match:
        // TODO: handle likely/unlikely
        build(bldr, ip->iff.thenbr);
        int sepidx = write_jmp(bldr, -1);
        build(bldr, ip->iff.elsebr);
        cod_vec_iter(jmps, i, x, bldr->arr[x].jze.offs = sepidx - x + 1);
        bldr->arr[sepidx].jmp.offs = bldr->len - sepidx;
        goto end_if;

end_if:
        cod_vec_destroy(jmps);
        break;
      }

      case ETH_INSN_MOV:
        write_dup(bldr, ip->out, ip->var.vid);
        break;

      case ETH_INSN_REF:
        write_ref(bldr, ip->var.vid);
        break;

      case ETH_INSN_DEC:
        write_dec(bldr, ip->var.vid);
        break;

      case ETH_INSN_UNREF:
        write_unref(bldr, ip->var.vid);
        break;

      case ETH_INSN_DROP:
        write_drop(bldr, ip->var.vid);
        break;

      case ETH_INSN_RET:
        write_ret(bldr, ip->var.vid);
        break;

      case ETH_INSN_BINOP:
        switch (ip->binop.op)
        {
          case ETH_ADD:
            write_add(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_SUB:
            write_sub(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_MUL:
            write_mul(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_DIV:
            write_div(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_LT:
            write_lt(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_LE:
            write_le(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_GT:
            write_gt(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_GE:
            write_ge(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_EQ:
            write_eq(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_NE:
            write_ne(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_IS:
            write_is(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;

          case ETH_CONS:
            write_cons(bldr, ip->out, ip->binop.lhs, ip->binop.rhs);
            break;
        }
        break;

      case ETH_INSN_FN:
      {
        eth_bytecode *bc = eth_build_bytecode(ip->fn.ssa);
        eth_ir *ir = ip->fn.ir;
        write_fn(bldr, ip->out, ip->fn.arity, ir, bc, ip->fn.caps, ip->fn.ncap);
        break;
      }

      case ETH_INSN_ALCFN:
        write_alcfn(bldr, ip->out, ip->alcfn.arity);
        break;

      case ETH_INSN_FINFN:
      {
        eth_bytecode *bc = eth_build_bytecode(ip->finfn.ssa);
        eth_ir *ir = ip->finfn.ir;
        write_finfn(bldr, ip->out, ip->finfn.arity, ir, bc, ip->finfn.caps,
            ip->finfn.ncap);
        break;
      }

      case ETH_INSN_POP:
        write_pop(bldr, ip->out, ip->pop.n);
        break;

      case ETH_INSN_CAP:
        write_cap(bldr, ip->out, ip->cap.n);
        break;

      case ETH_INSN_MKSCP:
        write_mkscp(bldr, ip->mkscp.clos, ip->mkscp.nclos, ip->mkscp.wrefs,
            ip->mkscp.nwref);
        break;
    }
  }
}

eth_bytecode*
eth_build_bytecode(eth_ssa *ssa)
{
  builder *bldr = create_builder();

  build(bldr, ssa->body);

  // finalize deffered blocks:
  for (size_t i = 0; i < bldr->deff.len; ++i)
  {
    deff_block *dblock = bldr->deff.data + i;
    int startpos = bldr->len;
    bldr->arr[dblock->jmppos].jmp.offs = startpos - dblock->jmppos;
    build(bldr, dblock->code);
    write_jmp(bldr, dblock->retpos);
  }

  eth_bytecode *bc = malloc(sizeof(eth_bytecode));
  bc->rc = 0;
  bc->nreg = ssa->nvals;
  bc->len = bldr->len;
  bc->code = bldr->arr;
  destroy_builder(bldr);

  return bc;
}

static void
destroy_bytecode(eth_bytecode *bc)
{
  for (int i = 0; i < bc->len; ++i)
    destroy_insn(bc->code + i);
  free(bc->code);
  free(bc);
}

void
eth_ref_bytecode(eth_bytecode *bc)
{
  bc->rc += 1;
}

void
eth_unref_bytecode(eth_bytecode *bc)
{
  if (--bc->rc == 0)
    destroy_bytecode(bc);
}

void
eth_drop_bytecode(eth_bytecode *bc)
{
  if (bc->rc == 0)
    destroy_bytecode(bc);
}

