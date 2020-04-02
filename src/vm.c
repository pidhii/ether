#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

ETH_MODULE("ether:vm")


eth_t
eth_vm(eth_bytecode *bc)
{
  const int nreg = bc->nreg;
  eth_t r[nreg];
  bool test = 0;
  int nstack = 0;
  eth_t exn = NULL;

  for (register eth_bc_insn *restrict ip = bc->code; true; ++ip)
  {
fetch_insn:
#ifdef ETH_DEBUG_MODE
    switch (ip->opc)
#else
    switch (ip->opc & 0x3F)
#endif
    {
      case ETH_OPC_CVAL:
        r[ip->cval.out] = ip->cval.val;
        break;

      case ETH_OPC_PUSH:
        eth_reserve_stack(ip->push.n);
        for (size_t i = 0; i < ip->push.n; ++i)
          eth_sp[i] = r[ip->push.vids[i]];
        nstack += ip->push.n;
        break;

      case ETH_OPC_POP:
        memcpy(r + ip->pop.vid0, eth_sp, sizeof(eth_t) * ip->pop.n);
        eth_pop_stack(ip->pop.n);
        break;

      case ETH_OPC_APPLY:
      {
        eth_t fn = r[ip->apply.fn];
        if (eth_unlikely(fn->type != eth_function_type))
        {
          while (nstack--) eth_drop(*eth_sp++);
          r[ip->apply.out] = eth_exn(eth_sym("Apply_error"));
          nstack = 0;
          break;
        }
        r[ip->apply.out] = eth_apply(fn, nstack);
        nstack = 0;
        break;
      }

      case ETH_OPC_APPLYTC:
      {
        eth_t fn = r[ip->apply.fn];
        if (eth_unlikely(fn->type != eth_function_type))
        {
          while (nstack--) eth_drop(*eth_sp++);
          r[ip->apply.out] = eth_exn(eth_sym("Apply_error"));
          nstack = 0;
          break;
        }

        eth_function *restrict func = ETH_FUNCTION(fn);
        if (func->islam && func->clos.bc->nreg <= nreg)
        {
          bc = func->clos.bc;
          ip = func->clos.bc->code;
          eth_this = func;
          eth_nargs = nstack;
          nstack = 0;
          goto fetch_insn; // to prevent increment of IP
        }
        else
        {
          r[ip->apply.out] = eth_apply(fn, nstack);
          nstack = 0;
        }
        break;
      }

      case ETH_OPC_TEST:
        test = r[ip->test.vid] != eth_false;
        break;

      case ETH_OPC_TESTTY:
        test = r[ip->testty.vid]->type == ip->testty.type;
        break;

      case ETH_OPC_TESTIS:
        test = r[ip->testty.vid] == ip->testis.cval;
        break;

      case ETH_OPC_GETTEST:
        r[ip->gettest.out] = eth_boolean(test);
        break;

      case ETH_OPC_DUP:
        r[ip->dup.out] = r[ip->dup.vid];
        break;

      case ETH_OPC_JNZ:
        if (test)
          ip += ip->jnz.offs - 1; // `-1` to compensate for loop increment
        break;

      case ETH_OPC_JZE:
        if (not test)
          ip += ip->jze.offs - 1; // `-1` to compensate for loop increment
        break;

      case ETH_OPC_JMP:
        ip += ip->jmp.offs - 1; // `-1` to compensate for loop increment
        break;

      case ETH_OPC_REF:
        eth_ref(r[ip->ref.vid]);
        break;

      case ETH_OPC_DEC:
        eth_dec(r[ip->dec.vid]);
        break;

      case ETH_OPC_UNREF:
        eth_unref(r[ip->unref.vid]);
        break;

      case ETH_OPC_DROP:
        eth_drop(r[ip->drop.vid]);
        break;

      case ETH_OPC_RET:
        return r[ip->ret.vid];

#define ARITHM_BINOP(opc, op)                                 \
      case ETH_OPC_##opc:                                     \
      {                                                       \
        eth_number_t lhs = ETH_NUMBER(r[ip->binop.lhs])->val; \
        eth_number_t rhs = ETH_NUMBER(r[ip->binop.rhs])->val; \
        r[ip->binop.out] = eth_num(op);                       \
        break;                                                \
      }
      ARITHM_BINOP(ADD, lhs + rhs)
      ARITHM_BINOP(SUB, lhs - rhs)
      ARITHM_BINOP(MUL, lhs * rhs)
      ARITHM_BINOP(DIV, lhs / rhs)
      ARITHM_BINOP(MOD, eth_mod(lhs, rhs))
      ARITHM_BINOP(POW, eth_pow(lhs, rhs))
      // ---
      ARITHM_BINOP(LAND, (intmax_t)lhs & (intmax_t)rhs)
      ARITHM_BINOP(LOR,  (intmax_t)lhs | (intmax_t)rhs)
      ARITHM_BINOP(LXOR, (intmax_t)lhs ^ (intmax_t)rhs)
      ARITHM_BINOP(LSHL, (uintmax_t)lhs << (uintmax_t)rhs)
      ARITHM_BINOP(LSHR, (uintmax_t)lhs >> (uintmax_t)rhs)
      ARITHM_BINOP(ASHL, (intmax_t)lhs << (intmax_t)rhs)
      ARITHM_BINOP(ASHR, (intmax_t)lhs >> (intmax_t)rhs)

#define ARITHM_CMP_BINOP(opc, op)                             \
      case ETH_OPC_##opc:                                     \
      {                                                       \
        eth_number_t lhs = ETH_NUMBER(r[ip->binop.lhs])->val; \
        eth_number_t rhs = ETH_NUMBER(r[ip->binop.rhs])->val; \
        r[ip->binop.out] = eth_boolean(op);                   \
        break;                                                \
      }
      ARITHM_CMP_BINOP(LT, lhs < rhs)
      ARITHM_CMP_BINOP(LE, lhs <= rhs)
      ARITHM_CMP_BINOP(GT, lhs > rhs)
      ARITHM_CMP_BINOP(GE, lhs >= rhs)
      ARITHM_CMP_BINOP(EQ, lhs == rhs)
      ARITHM_CMP_BINOP(NE, lhs != rhs)

      case ETH_OPC_IS:
        r[ip->binop.out] = eth_boolean(r[ip->binop.lhs] == r[ip->binop.rhs]);
        break;

      case ETH_OPC_CONS:
        r[ip->binop.out] = eth_cons(r[ip->binop.lhs], r[ip->binop.rhs]);
        break;

      case ETH_OPC_NOT:
        r[ip->unop.out] = eth_boolean(r[ip->unop.vid] == eth_false);
        break;

      case ETH_OPC_LNOT:
        r[ip->unop.out] = eth_num(~(intmax_t)ETH_NUMBER(r[ip->unop.vid])->val);
        break;

      case ETH_OPC_FN:
      {
        size_t ncap = ip->fn.data->ncap;
        eth_t *cap = malloc(sizeof(eth_t) * ncap);
        for (size_t i = 0; i < ncap; ++i)
          cap[i] = r[ip->fn.data->caps[i]];
        eth_t fn = eth_create_clos(ip->fn.data->ir, ip->fn.data->bc, cap, ncap,
            ip->fn.data->arity);
        r[ip->fn.out] = fn;
        break;
      }

      case ETH_OPC_FINFN:
      {
        size_t ncap = ip->fn.data->ncap;
        eth_t *cap = malloc(sizeof(eth_t) * ncap);
        for (size_t i = 0; i < ncap; ++i)
          cap[i] = r[ip->fn.data->caps[i]];
        eth_function *fn = ETH_FUNCTION(r[ip->fn.out]);
        eth_finalize_clos(fn, ip->fn.data->ir, ip->fn.data->bc, cap, ncap,
            ip->fn.data->arity);
        break;
      }

      case ETH_OPC_ALCFN:
        r[ip->alcfn.out] = eth_create_dummy_func(ip->alcfn.arity);
        break;

      case ETH_OPC_CAP:
        memcpy(r + ip->cap.vid0, eth_this->clos.cap, sizeof(eth_t) * ip->cap.n);
        break;

      case ETH_OPC_MKSCP:
      {
        eth_t wrefs[ip->mkscp.data->nwref];
        eth_function *clos[ip->mkscp.data->nclos];
        for (size_t i = 0; i < ip->mkscp.data->nwref; ++i)
          wrefs[i] = r[ip->mkscp.data->wrefs[i]];
        for (size_t i = 0; i < ip->mkscp.data->nclos; ++i)
          clos[i] = ETH_FUNCTION(r[ip->mkscp.data->clos[i]]);
        eth_create_scp(clos, ip->mkscp.data->nclos, wrefs, ip->mkscp.data->nwref);
        break;
      }

      case ETH_OPC_LOAD:
        r[ip->load.out] = *(eth_t*) ((char*)r[ip->load.vid] + ip->load.offs);
        break;

      case ETH_OPC_SETEXN:
        exn = r[ip->setexn.vid];
        break;

      case ETH_OPC_GETEXN:
      {
        eth_t what = ETH_EXCEPTION(exn)->what;
        eth_ref(what);
        eth_unref(exn);
        r[ip->getexn.out] = what;
        break;
      }

      case ETH_OPC_MKRCRD:
      {
        eth_type *type = ip->mkrcrd.data->type;
        uint64_t *vids = ip->mkrcrd.data->vids;
        int n = type->nfields;
        assert(n > 0);
        eth_tuple *rec;
        switch (n)
        {
          case 1:  rec = eth_alloc_h1(); break;
          case 2:  rec = eth_alloc_h2(); break;
          case 3:  rec = eth_alloc_h3(); break;
          case 4:  rec = eth_alloc_h4(); break;
          case 5:  rec = eth_alloc_h5(); break;
          case 6:  rec = eth_alloc_h6(); break;
          default: rec = malloc(sizeof(eth_tuple) + sizeof(eth_t) * n);
        }
        eth_init_header(rec, type);
        for (int i = 0; i < n; ++i)
          rec->data[i] = r[vids[i]];
        r[ip->mkrcrd.out] = ETH(rec);
        break;
      }
    }
  }
}

