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
#include "codeine/vec.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>


ETH_MODULE("ether:bytecode")


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
      eth_unref_source(insn->fn.data->src);
      eth_unref_bytecode(insn->fn.data->bc);
      free(insn->fn.data);
      break;

    case ETH_OPC_MKSCP:
      free(insn->mkscp.data->clos);
      free(insn->mkscp.data);
      break;

    case ETH_OPC_TESTIS:
      eth_unref(insn->testis.cval);
      break;

    case ETH_OPC_MKRCRD:
      free(insn->mkrcrd.vids);
      break;

    case ETH_OPC_LOADRCRD:
      free(insn->loadrcrd.vids);
      free(insn->loadrcrd.ids);
      break;

    case ETH_OPC_LOOP:
      free(insn->loop.vids);
      break;

    case ETH_OPC_UPDTRCRD:
      free(insn->updtrcrd.vids);
      free(insn->updtrcrd.ids);
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
  int pos, tryid;
} catch_jmp;

typedef struct {
  int len, cap; /* Length and capacity of the code tape. */
  eth_bc_insn *arr; /* Code(output)-tape. */
  cod_vec(deff_block) deff; /* Deffered blocks. */
  cod_vec(catch_jmp) cchjmps;
  int *catches;
  int *vmap; /* Map of the SSA-values to registers. */
  int regcnt;
  int entrypoint;
} bc_builder;

static bc_builder*
create_bc_builder(int nvals, int ntries)
{
  bc_builder *bldr = malloc(sizeof(bc_builder));
  bldr->len = 0;
  bldr->cap = 0x40;
  bldr->arr = malloc(sizeof(eth_bc_insn) * bldr->cap);
  cod_vec_init(bldr->deff);
  cod_vec_init(bldr->cchjmps);
  bldr->catches = malloc(sizeof(int) * ntries);
  bldr->vmap = malloc(sizeof(int) * nvals);
  for (int i = 0; i < nvals; bldr->vmap[i++] = -1);
  bldr->regcnt = 0;
  bldr->entrypoint = -1;
  return bldr;
}

static void
destroy_bc_builder(bc_builder *bldr)
{
  cod_vec_destroy(bldr->deff);
  cod_vec_destroy(bldr->cchjmps);
  free(bldr->catches);
  free(bldr->vmap);
  free(bldr);
}

static int
new_reg(bc_builder *bldr, int vid)
{
  int reg = bldr->regcnt++;
  bldr->vmap[vid] = reg;
  return reg;
}

static int
get_reg(bc_builder *bldr, int vid)
{
  assert(vid >= 0);
  assert(bldr->vmap[vid] >= 0);
  return bldr->vmap[vid];
}

static eth_bc_insn*
append_insn(bc_builder *bldr)
{
  if (eth_unlikely(bldr->cap == bldr->len))
  {
    bldr->cap <<= 1;
    bldr->arr = realloc(bldr->arr, sizeof(eth_bc_insn) * bldr->cap);
  }
  return bldr->arr + bldr->len++;
}

static int
write_cval(bc_builder *bldr, int out, eth_t val)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_CVAL;
  insn->cval.out = out;
  eth_ref(insn->cval.val = val);
  return bldr->len - 1;
}

static int
write_push(bc_builder *bldr, int *vids, int n)
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
write_pop(bc_builder *bldr, int vid0, int n)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_POP;
  insn->pop.vid0 = vid0;
  insn->pop.n = n;
  return bldr->len - 1;
}

static int
write_apply(bc_builder *bldr, int out, int fn)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_APPLY;
  insn->apply.out = out;
  insn->apply.fn = fn;
  return bldr->len - 1;
}

static int
write_applytc(bc_builder *bldr, int out, int fn)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_APPLYTC;
  insn->apply.out = out;
  insn->apply.fn = fn;
  return bldr->len - 1;
}

static int
write_loop(bc_builder *bldr, ptrdiff_t offs, const int args[], int n)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_LOOP;
  insn->loop.offs = offs;
  insn->loop.vids = malloc(sizeof(uint64_t) * n);
  insn->loop.n = n;
  for (int i = 0; i < n; ++i)
    insn->loop.vids[i] = args[i];
  return bldr->len - 1;
}

static int
write_test(bc_builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_TEST;
  insn->test.vid = vid;
  return bldr->len - 1;
}

static int
write_testty(bc_builder *bldr, int vid, eth_type *type)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_TESTTY;
  insn->testty.vid = vid;
  insn->testty.type = type;
  return bldr->len - 1;
}

static int
write_testis(bc_builder *bldr, int vid, eth_t cval)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_TESTIS;
  insn->testis.vid = vid;
  insn->testis.cval = cval;
  eth_ref(cval);
  return bldr->len - 1;
}

static int
write_testequal(bc_builder *bldr, int vid, eth_t cval)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_TESTEQUAL;
  insn->testequal.vid = vid;
  insn->testequal.cval = cval;
  eth_ref(cval);
  return bldr->len - 1;
}

static int
write_gettest(bc_builder *bldr, int out)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_GETTEST;
  insn->gettest.out = out;
  return bldr->len - 1;
}

static int
write_dup(bc_builder *bldr, int out, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_DUP;
  insn->dup.out = out;
  insn->dup.vid = vid;
  return bldr->len - 1;
}

static int
write_jnz(bc_builder *bldr, ptrdiff_t offs)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_JNZ;
  insn->jnz.offs = offs;
  return bldr->len - 1;
}

static int
write_jze(bc_builder *bldr, ptrdiff_t offs)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_JZE;
  insn->jze.offs = offs;
  return bldr->len - 1;
}

static int
write_jmp(bc_builder *bldr, ptrdiff_t offs)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_JMP;
  insn->jmp.offs = offs;
  return bldr->len - 1;
}

static int
write_ref(bc_builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_REF;
  insn->ref.vid = vid;
  return bldr->len - 1;
}

static int
write_dec(bc_builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_DEC;
  insn->dec.vid = vid;
  return bldr->len - 1;
}

static int
write_unref(bc_builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_UNREF;
  insn->unref.vid = vid;
  return bldr->len - 1;
}

static int
write_drop(bc_builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_DROP;
  insn->drop.vid = vid;
  return bldr->len - 1;
}

static int
write_ret(bc_builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_RET;
  insn->ret.vid = vid;
  return bldr->len - 1;
}

static int
write_fn(bc_builder *bldr, int out, int arity, eth_source *src, eth_bytecode *bc,
    int *caps, int ncap)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_FN;
  insn->fn.out = out;
  insn->fn.data = malloc(sizeof(insn->fn.data[0]) + sizeof(int) * ncap);
  insn->fn.data->arity = arity;
  insn->fn.data->src = src;
  insn->fn.data->bc = bc;
  insn->fn.data->ncap = ncap;
  eth_ref_source(src);
  eth_ref_bytecode(bc);
  memcpy(insn->fn.data->caps, caps, sizeof(int) * ncap);
  return bldr->len - 1;
}

static int
write_alcfn(bc_builder *bldr, int out, int arity)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_ALCFN;
  insn->alcfn.out = out;
  insn->alcfn.arity = arity;
  return bldr->len - 1;
}

static int
write_cap(bc_builder *bldr, int vid0, int n)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_CAP;
  insn->cap.vid0 = vid0;
  insn->cap.n = n;
  return bldr->len - 1;
}

static int
write_finfn(bc_builder *bldr, int out, int arity, eth_source *src,
    eth_bytecode *bc, int *caps, int ncap)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_FINFN;
  insn->fn.out = out;
  insn->fn.data = malloc(sizeof(insn->fn.data[0]) + sizeof(int) * ncap);
  insn->fn.data->arity = arity;
  insn->fn.data->src = src;
  insn->fn.data->bc = bc;
  insn->fn.data->ncap = ncap;
  eth_ref_source(src);
  eth_ref_bytecode(bc);
  memcpy(insn->fn.data->caps, caps, sizeof(int) * ncap);
  return bldr->len - 1;
}

static int
write_mkscp(bc_builder *bldr, int *clos, int nclos)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_MKSCP;
  insn->mkscp.data = malloc(sizeof *insn->mkscp.data);
  insn->mkscp.data->clos = malloc(sizeof(size_t) * nclos);
  insn->mkscp.data->nclos = nclos;
  for (int i = 0; i < nclos; ++i)
    insn->mkscp.data->clos[i] = clos[i];
  return bldr->len - 1;
}

static int
write_load(bc_builder *bldr, int out, int vid, int offs)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_LOAD;
  insn->load.out = out;
  insn->load.vid = vid;
  insn->load.offs = offs;
  return bldr->len - 1;
}

static int
write_loadrcrd(bc_builder *bldr, size_t *ids, int *vids, int n, int src)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_LOADRCRD;
  insn->loadrcrd.ids = malloc(sizeof(size_t) * n);
  insn->loadrcrd.vids = malloc(sizeof(uint64_t) * n);
  insn->loadrcrd.n = n;
  insn->loadrcrd.src = src;
  for (int i = 0; i < n; ++i)
  {
    insn->loadrcrd.ids[i] = ids[i];
    insn->loadrcrd.vids[i] = vids[i];
  }
  return bldr->len - 1;
}

static int
write_loadrcrd1(bc_builder *bldr, int out, int vid, size_t id)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_LOADRCRD1;
  insn->loadrcrd1.out = out;
  insn->loadrcrd1.vid = vid;
  insn->loadrcrd1.id  = id;
  return bldr->len - 1;
}

static int
write_setexn(bc_builder *bldr, int vid)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_SETEXN;
  insn->setexn.vid = vid;
  return bldr->len - 1;
}

static int
write_getexn(bc_builder *bldr, int out)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_GETEXN;
  insn->getexn.out = out;
  return bldr->len - 1;
}

static int
write_mkrcrd(bc_builder *bldr, int out, int const vids[], eth_type *type)
{
  int n = type->nfields;
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_MKRCRD;
  insn->mkrcrd.out = out;
  insn->mkrcrd.type = type;
  insn->mkrcrd.vids = malloc(sizeof(uint64_t) * n);
  for (int i = 0; i < n; ++i)
    insn->mkrcrd.vids[i] = vids[i];
  return bldr->len - 1;
}

static int
write_updtrcrd(bc_builder *bldr, int out, int src, int const vids[],
    size_t const ids[], int n)
{
  eth_bc_insn *insn = append_insn(bldr);
  insn->opc = ETH_OPC_UPDTRCRD;
  insn->updtrcrd.out = out;
  insn->updtrcrd.src = src;
  insn->updtrcrd.n = n;
  insn->updtrcrd.vids = malloc(sizeof(uint64_t) * n);
  insn->updtrcrd.ids = malloc(sizeof(size_t) * n);
  for (int i = 0; i < n; ++i)
  {
    insn->updtrcrd.vids[i] = vids[i];
    insn->updtrcrd.ids[i] = ids[i];
  }
  return bldr->len - 1;
}

#define MAKE_BINOP(name, op)                                \
  static int                                                \
  write_##name(bc_builder *bldr, int out, int lhs, int rhs) \
  {                                                         \
    eth_bc_insn *insn = append_insn(bldr);                  \
    insn->opc = ETH_OPC_##op;                               \
    insn->binop.out = out;                                  \
    insn->binop.lhs = lhs;                                  \
    insn->binop.rhs = rhs;                                  \
    return bldr->len - 1;                                   \
  }
MAKE_BINOP(add , ADD)
MAKE_BINOP(sub , SUB)
MAKE_BINOP(mul , MUL)
MAKE_BINOP(div , DIV)
MAKE_BINOP(mod , MOD)
MAKE_BINOP(pow , POW)
MAKE_BINOP(land, LAND)
MAKE_BINOP(lor , LOR)
MAKE_BINOP(lxor, LXOR)
MAKE_BINOP(lshl, LSHL)
MAKE_BINOP(lshr, LSHR)
MAKE_BINOP(ashl, ASHL)
MAKE_BINOP(ashr, ASHR)
MAKE_BINOP(lt  , LT)
MAKE_BINOP(le  , LE)
MAKE_BINOP(gt  , GT)
MAKE_BINOP(ge  , GE)
MAKE_BINOP(eq  , EQ)
MAKE_BINOP(ne  , NE)
MAKE_BINOP(is  , IS)
MAKE_BINOP(equal, EQUAL)
MAKE_BINOP(cons, CONS)

#define MAKE_UNOP(name, op)                        \
  static int                                       \
  write_##name(bc_builder *bldr, int out, int vid) \
  {                                                \
    eth_bc_insn *insn = append_insn(bldr);         \
    insn->opc = ETH_OPC_##op;                      \
    insn->unop.out = out;                          \
    insn->unop.vid = vid;                          \
    return bldr->len - 1;                          \
  }
MAKE_UNOP(not , NOT)
MAKE_UNOP(lnot, LNOT)



typedef cod_vec(int) int_vec;

// TODO: optimize (first unpacks, then idents)
// XXX: all jumps written here must be 'jze's
static void
build_pattern(bc_builder *bldr, eth_ssa_pattern *pat, int expr, int_vec *jmps)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
      break;

    case ETH_PATTERN_IDENT:
      break;

    case ETH_PATTERN_CONSTANT:
      if (pat->constant.dotest)
      {
        switch (pat->constant.testop)
        {
          case ETH_TEST_IS:
            write_testis(bldr, expr, pat->constant.val);
            break;

          case ETH_TEST_EQUAL:
            write_testequal(bldr, expr, pat->constant.val);
            break;
        }
        int jmp = write_jze(bldr, -1);
        cod_vec_push(*jmps, jmp);
      }
      else
        eth_debug("ommiting EQ test for ~w", pat->constant.val);
      break;

    case ETH_PATTERN_UNPACK:
    {
      if (pat->unpack.dotest)
      {
        write_testty(bldr, expr, pat->unpack.type);
        int jmp = write_jze(bldr, -1);
        cod_vec_push(*jmps, jmp);
      }
      else
        eth_debug("ommiting type-check for %s", pat->unpack.type->name);

      for (int i = 0; i < pat->unpack.n; ++i)
      {
        if (pat->unpack.subpat[i]->tag == ETH_PATTERN_DUMMY)
          continue;
        int oreg = new_reg(bldr, pat->unpack.vids[i]);
        write_load(bldr, oreg, expr, pat->unpack.offs[i]);
        build_pattern(bldr, pat->unpack.subpat[i], oreg, jmps);
      }

      break;
    }

    case ETH_PATTERN_RECORD:
    {
      assert(pat->record.n > 0);
      // --
      int n = pat->record.n;
      int oregs[n];
      // --
      for (int i = 0; i < n; ++i)
        oregs[i] = new_reg(bldr, pat->record.vids[i]);

      // only load non-reused fields
      if (n == 1)
        write_loadrcrd1(bldr, oregs[0], expr, pat->record.ids[0]);
      else
        write_loadrcrd(bldr, pat->record.ids, oregs, n, expr);

      if (pat->record.dotest)
      {
        int jmp = write_jze(bldr, -1);
        cod_vec_push(*jmps, jmp);
      }
      else
        eth_debug("ommiting test for record");

      for (int i = 0; i < n; ++i)
        build_pattern(bldr, pat->record.subpat[i], oregs[i], jmps);

      break;
    }
  }
}

static void
build(bc_builder *bldr, eth_insn *ssa)
{
  for (eth_insn *ip = ssa; ip; ip = ip->next)
  {
    if (ip->flag & ETH_IFLAG_ENTRYPOINT)
      bldr->entrypoint = bldr->len;

    switch (ip->tag)
    {
      case ETH_INSN_NOP:
        assert((ip->flag & ETH_IFLAG_ENTRYPOINT) == 0);
        break;

      case ETH_INSN_CVAL:
        write_cval(bldr, new_reg(bldr, ip->out), ip->cval.val);
        break;

      case ETH_INSN_APPLY:
      {
        int nargs = ip->apply.nargs;
        int args[nargs];
        for (int i = 0; i < nargs; ++i)
          args[i] = get_reg(bldr, ip->apply.args[i]);
        write_push(bldr, args, nargs);

        int out = new_reg(bldr, ip->out);
        int fn = get_reg(bldr, ip->apply.fn);
        write_apply(bldr, out, fn);
        break;
      }

      case ETH_INSN_APPLYTC:
      {
        int nargs = ip->apply.nargs;
        int args[nargs];
        for (int i = 0; i < nargs; ++i)
          args[i] = get_reg(bldr, ip->apply.args[i]);
        write_push(bldr, args, nargs);

        int out = new_reg(bldr, ip->out);
        int fn = get_reg(bldr, ip->apply.fn);
        write_applytc(bldr, out, fn);
        break;
      }

      case ETH_INSN_LOOP:
      {
        int n = ip->loop.nargs;
        int vids[n];
        for (int i = 0; i < n; ++i)
          vids[i] = get_reg(bldr, ip->loop.args[i]);
        assert(bldr->entrypoint >= 0);
        int offs = bldr->entrypoint - bldr->len;
        write_loop(bldr, offs, vids, n);
        break;
      }

      case ETH_INSN_IF:
      {
        int_vec jmps;
        cod_vec_init(jmps);

        // create PHI
        int phi = -1;
        if (ip->out >= 0)
          phi = new_reg(bldr, ip->out);

        // test:
        int cond = get_reg(bldr, ip->iff.cond);
        switch (ip->iff.test)
        {
          case ETH_TEST_NOTFALSE:
            write_test(bldr, cond);
            goto if_notfalse;

          case ETH_TEST_TYPE:
            write_testty(bldr, cond, ip->iff.type);
            goto if_type;

          case ETH_TEST_MATCH:
            build_pattern(bldr, ip->iff.pat, cond, &jmps);
            goto if_match;

          case ETH_TEST_UPDATE:
            assert(phi >= 0);
            int n = ip->iff.update.n;
            int vids[n];
            for (int i = 0; i < n; ++i)
              vids[i] = get_reg(bldr, ip->iff.update.vids[i]);
            write_updtrcrd(bldr, phi, cond, vids, ip->iff.update.ids, n);
            goto if_update;
        }

if_notfalse:
if_type:
if_update:
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
        if (jmps.len == 0)
        {
          eth_debug("ommiting else-branch in MATCH");
          build(bldr, ip->iff.thenbr);
        }
        else
        {
          if (ip->iff.likely > 0)
          {
            build(bldr, ip->iff.thenbr);
            cod_vec_iter(jmps, i, x,
              cod_vec_push(bldr->deff, ((deff_block) {
                  .jmppos = x,
                  .retpos = bldr->len,
                  .code = ip->iff.elsebr,
              }));
            );
          }
          else if (ip->iff.likely < 0)
          {
            build(bldr, ip->iff.elsebr);
            cod_vec_iter(jmps, i, x,
              // change JZEs to JNZs
              assert(bldr->arr[x].opc == ETH_OPC_JZE);
              bldr->arr[x].opc = ETH_OPC_JNZ;
              cod_vec_push(bldr->deff, ((deff_block) {
                  .jmppos = x,
                  .retpos = bldr->len,
                  .code = ip->iff.thenbr,
              }));
            );
          }
          else
          {
            build(bldr, ip->iff.thenbr);
            int sepidx = write_jmp(bldr, -1);
            build(bldr, ip->iff.elsebr);
            cod_vec_iter(jmps, i, x, bldr->arr[x].jze.offs = sepidx - x + 1);
            bldr->arr[sepidx].jmp.offs = bldr->len - sepidx;
          }
        }
        goto end_if;

end_if:
        cod_vec_destroy(jmps);
        break;
      }

      case ETH_INSN_MOV:
        write_dup(bldr, get_reg(bldr, ip->out), get_reg(bldr, ip->var.vid));
        break;

      case ETH_INSN_REF:
        write_ref(bldr, get_reg(bldr, ip->var.vid));
        break;

      case ETH_INSN_DEC:
        write_dec(bldr, get_reg(bldr, ip->var.vid));
        break;

      case ETH_INSN_UNREF:
        write_unref(bldr, get_reg(bldr, ip->var.vid));
        break;

      case ETH_INSN_DROP:
        write_drop(bldr, get_reg(bldr, ip->var.vid));
        break;

      case ETH_INSN_RET:
      {
        if (ip->var.vid >= 0)
          write_ret(bldr, get_reg(bldr, ip->var.vid));
        break;
      }

      case ETH_INSN_BINOP:
        switch (ip->binop.op)
        {
#define WRITE_BINOP(name, opc)                      \
          case ETH_##opc:                           \
          {                                         \
            int out = new_reg(bldr, ip->out);       \
            int lhs = get_reg(bldr, ip->binop.lhs); \
            int rhs = get_reg(bldr, ip->binop.rhs); \
            write_##name(bldr, out, lhs, rhs);      \
            break;                                  \
          }
          WRITE_BINOP(add, ADD)
          WRITE_BINOP(sub, SUB)
          WRITE_BINOP(mul, MUL)
          WRITE_BINOP(div, DIV)
          WRITE_BINOP(mod, MOD)
          WRITE_BINOP(pow, POW)
          // ---
          WRITE_BINOP(land, LAND)
          WRITE_BINOP(lor,  LOR)
          WRITE_BINOP(lxor, LXOR)
          WRITE_BINOP(lshl, LSHL)
          WRITE_BINOP(lshr, LSHR)
          WRITE_BINOP(ashl, ASHL)
          WRITE_BINOP(ashr, ASHR)
          // ---
          WRITE_BINOP(lt, LT)
          WRITE_BINOP(le, LE)
          WRITE_BINOP(gt, GT)
          WRITE_BINOP(ge, GE)
          WRITE_BINOP(eq, EQ)
          WRITE_BINOP(ne, NE)
          // ---
          WRITE_BINOP(is, IS)
          WRITE_BINOP(equal, EQUAL)
          // ---
          WRITE_BINOP(cons, CONS)
        }
        break;

      case ETH_INSN_UNOP:
      {
        int out = new_reg(bldr, ip->out);
        int reg = get_reg(bldr, ip->unop.vid);
        switch (ip->unop.op)
        {
          case ETH_NOT:
            write_not(bldr, out, reg);
            break;

          case ETH_LNOT:
            write_lnot(bldr, out, reg);
            break;
        }
        break;
      }

      case ETH_INSN_FN:
      {
        eth_bytecode *bc = eth_build_bytecode(ip->fn.ssa);
        eth_ir *ir = ip->fn.ir;
        int out = new_reg(bldr, ip->out);
        int ncap = ip->fn.ncap;
        int caps[ncap];
        for (int i = 0; i < ncap; ++i)
          caps[i] = get_reg(bldr, ip->fn.caps[i]);
        eth_source *src = eth_create_source(ip->fn.ast, ip->fn.ir, ip->fn.ssa);
        write_fn(bldr, out, ip->fn.arity, src, bc, caps, ncap);
        break;
      }

      case ETH_INSN_ALCFN:
        write_alcfn(bldr, new_reg(bldr, ip->out), ip->alcfn.arity);
        break;

      case ETH_INSN_FINFN:
      {
        eth_bytecode *bc = eth_build_bytecode(ip->finfn.ssa);
        eth_ir *ir = ip->finfn.ir;
        int out = get_reg(bldr, ip->out);
        int ncap = ip->fn.ncap;
        int caps[ncap];
        for (int i = 0; i < ncap; ++i)
          caps[i] = get_reg(bldr, ip->finfn.caps[i]);
        eth_source *src = eth_create_source(ip->finfn.ast, ip->finfn.ir,
            ip->finfn.ssa);
        write_finfn(bldr, out, ip->finfn.arity, src, bc, caps, ncap);
        break;
      }

      case ETH_INSN_POP:
      {
        int n = ip->pop.n;
        int regs[n];
        for (int i = 0; i < n; ++i)
        {
          regs[i] = new_reg(bldr, ip->pop.vids[i]);
          assert(regs[i] == i);
        }
        write_pop(bldr, regs[0], n);
        break;
      }

      case ETH_INSN_CAP:
      {
        int n = ip->pop.n;
        int regs[n];
        for (int i = 0; i < n; ++i)
        {
          regs[i] = new_reg(bldr, ip->pop.vids[i]);
          assert(regs[i] == regs[0] + i);
        }
        write_cap(bldr, regs[0], n);
        break;
      }

      case ETH_INSN_MKSCP:
      {
        int nclos = ip->mkscp.nclos;
        int clos[nclos];
        for (int i = 0; i < nclos; ++i)
          clos[i] = get_reg(bldr, ip->mkscp.clos[i]);
        write_mkscp(bldr, clos, nclos);
        break;
      }

      // TODO: handle likely/unlikely
      case ETH_INSN_TRY:
        if (ip->out >= 0)
          new_reg(bldr, ip->out);
        build(bldr, ip->try.trybr);
        int jmpidx = write_jmp(bldr, -1);
        int cchidx = jmpidx + 1;
        build(bldr, ip->try.catchbr);
        bldr->arr[jmpidx].jmp.offs = bldr->len - jmpidx;
        bldr->catches[ip->try.id] = cchidx;
        break;

      case ETH_INSN_CATCH:
      {
        write_setexn(bldr, get_reg(bldr, ip->catch.vid));
        int jmpidx = write_jmp(bldr, -1);
        catch_jmp jmp = { .pos = jmpidx, .tryid = ip->catch.tryid };
        cod_vec_push(bldr->cchjmps, jmp);
        break;
      }

      case ETH_INSN_GETEXN:
        write_getexn(bldr, new_reg(bldr, ip->out));
        break;

      case ETH_INSN_MKRCRD:
      {
        int n = ip->mkrcrd.type->nfields;
        int out = new_reg(bldr, ip->out);
        int regs[n];
        for (int i = 0; i < n; ++i)
          regs[i] = get_reg(bldr, ip->mkrcrd.vids[i]);
        write_mkrcrd(bldr, out, regs, ip->mkrcrd.type);
        break;
      }
    }
  }
}

eth_bytecode*
eth_build_bytecode(eth_ssa *ssa)
{
  bc_builder *bldr = create_bc_builder(ssa->nvals, ssa->ntries);

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

  // set up catch-jumps
  for (size_t i = 0; i < bldr->cchjmps.len; ++i)
  {
    catch_jmp *jmp = bldr->cchjmps.data + i;
    bldr->arr[jmp->pos].jmp.offs = bldr->catches[jmp->tryid] - jmp->pos;;
  }

  eth_bytecode *bc = malloc(sizeof(eth_bytecode));
  bc->rc = 0;
  bc->nreg = ssa->nvals;
  bc->len = bldr->len;
  bc->code = bldr->arr;
  destroy_bc_builder(bldr);

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

