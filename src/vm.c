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
#include <math.h>

ETH_MODULE("ether:vm")


eth_t
eth_vm(eth_bytecode *bc)
{
  const int nreg = bc->nreg;

  if (eth_unlikely(not eth_reserve_c_stack(nreg * sizeof(eth_t))))
  {
    eth_warning("stack overflow");
    int nargs = eth_nargs;
    for (int i = 0; i < nargs; ++i)
      eth_ref(eth_sp[i]);
    for (int i = 0; i < nargs; ++i)
      eth_unref(eth_sp[i]);
    eth_pop_stack(nargs);
    return eth_exn(eth_sym("stack_overflow"));
  }

  eth_t *restrict r = alloca(nreg * sizeof(eth_t));
  bool test = 0;
  int nstack = 0;
  eth_t exn = NULL;

  register eth_bc_insn *restrict ip = bc->code;
  static void *insn[] = {
    [ETH_OPC_CVAL     ] = &&INSN_CVAL,
    [ETH_OPC_PUSH     ] = &&INSN_PUSH,
    [ETH_OPC_POP      ] = &&INSN_POP,
    [ETH_OPC_APPLY    ] = &&INSN_APPLY,
    [ETH_OPC_APPLYTC  ] = &&INSN_APPLYTC,
    [ETH_OPC_TEST     ] = &&INSN_TEST,
    [ETH_OPC_TESTTY   ] = &&INSN_TESTTY,
    [ETH_OPC_TESTIS   ] = &&INSN_TESTIS,
    [ETH_OPC_TESTEQUAL] = &&INSN_TESTEQUAL,
    [ETH_OPC_GETTEST  ] = &&INSN_GETTEST,
    [ETH_OPC_DUP      ] = &&INSN_DUP,
    [ETH_OPC_JNZ      ] = &&INSN_JNZ,
    [ETH_OPC_JZE      ] = &&INSN_JZE,
    [ETH_OPC_JMP      ] = &&INSN_JMP,
    [ETH_OPC_LOOP     ] = &&INSN_LOOP,
    [ETH_OPC_REF      ] = &&INSN_REF,
    [ETH_OPC_DEC      ] = &&INSN_DEC,
    [ETH_OPC_UNREF    ] = &&INSN_UNREF,
    [ETH_OPC_DROP     ] = &&INSN_DROP,
    [ETH_OPC_RET      ] = &&INSN_RET,
    [ETH_OPC_ADD      ] = &&INSN_ADD,
    [ETH_OPC_SUB      ] = &&INSN_SUB,
    [ETH_OPC_MUL      ] = &&INSN_MUL,
    [ETH_OPC_DIV      ] = &&INSN_DIV,
    [ETH_OPC_MOD      ] = &&INSN_MOD,
    [ETH_OPC_POW      ] = &&INSN_POW,
    [ETH_OPC_LAND     ] = &&INSN_LAND,
    [ETH_OPC_LOR      ] = &&INSN_LOR,
    [ETH_OPC_LXOR     ] = &&INSN_LXOR,
    [ETH_OPC_LSHL     ] = &&INSN_LSHL,
    [ETH_OPC_LSHR     ] = &&INSN_LSHR,
    [ETH_OPC_ASHL     ] = &&INSN_ASHL,
    [ETH_OPC_ASHR     ] = &&INSN_ASHR,
    [ETH_OPC_LT       ] = &&INSN_LT,
    [ETH_OPC_LE       ] = &&INSN_LE,
    [ETH_OPC_GT       ] = &&INSN_GT,
    [ETH_OPC_GE       ] = &&INSN_GE,
    [ETH_OPC_EQ       ] = &&INSN_EQ,
    [ETH_OPC_NE       ] = &&INSN_NE,
    [ETH_OPC_IS       ] = &&INSN_IS,
    [ETH_OPC_EQUAL    ] = &&INSN_EQUAL,
    [ETH_OPC_CONS     ] = &&INSN_CONS,
    [ETH_OPC_NOT      ] = &&INSN_NOT,
    [ETH_OPC_LNOT     ] = &&INSN_LNOT,
    [ETH_OPC_FN       ] = &&INSN_FN,
    [ETH_OPC_FINFN    ] = &&INSN_FINFN,
    [ETH_OPC_ALCFN    ] = &&INSN_ALCFN,
    [ETH_OPC_CAP      ] = &&INSN_CAP,
    [ETH_OPC_MKSCP    ] = &&INSN_MKSCP,
    [ETH_OPC_LOAD     ] = &&INSN_LOAD,
    [ETH_OPC_LOADRCRD ] = &&INSN_LOADRCRD,
    [ETH_OPC_LOADRCRD1] = &&INSN_LOADRCRD1,
    [ETH_OPC_SETEXN   ] = &&INSN_SETEXN,
    [ETH_OPC_GETEXN   ] = &&INSN_GETEXN,
    [ETH_OPC_MKRCRD   ] = &&INSN_MKRCRD,
    [ETH_OPC_UPDTRCRD ] = &&INSN_UPDTRCRD,
  };
#define FAST_DISPATCH_NEXT() goto *insn[(++ip)->opc]
#define FAST_DISPATCH() goto *insn[ip->opc]

#define DISPATCH_NEXT() break
#define DISPATCH() continue

#define PREDICTED(opc) PREDICTED_##opc:
#define PREDICT_NEXT(pred)             \
  do {                                 \
    if ((++ip)->opc == ETH_OPC_##pred) \
      goto PREDICTED_##pred;           \
    --ip;                              \
  } while (0)
#define PREDICT(pred)                  \
  do {                                 \
    if (ip->opc == ETH_OPC_##pred)     \
      goto PREDICTED_##pred;           \
  } while (0)

#define OP(opc) \
  INSN_##opc:   \
  case ETH_OPC_##opc:

  FAST_DISPATCH();
  for (;;)
  {
    switch (ip->opc)
    {
      OP(CVAL)
      {
        r[ip->cval.out] = ip->cval.val;
        FAST_DISPATCH_NEXT();
      }

      OP(PUSH)
      {
        eth_reserve_stack(ip->push.n);
        for (size_t i = 0; i < ip->push.n; ++i)
          eth_sp[i] = r[ip->push.vids[i]];
        nstack += ip->push.n;
        ip += 1;
        PREDICT(APPLY);
        PREDICT(APPLYTC);
        DISPATCH();
      }

      PREDICTED(POP)
      OP(POP)
      {
        memcpy(r + ip->pop.vid0, eth_sp, sizeof(eth_t) * ip->pop.n);
        eth_pop_stack(ip->pop.n);
        FAST_DISPATCH_NEXT();
      }

      PREDICTED(APPLY)
      OP(APPLY)
      {
        eth_t fn = r[ip->apply.fn];
        if (eth_unlikely(fn->type != eth_function_type))
        {
          while (nstack--) eth_drop(*eth_sp++);
          r[ip->apply.out] = eth_exn(eth_sym("apply_error"));
          nstack = 0;
          FAST_DISPATCH_NEXT();
        }
        r[ip->apply.out] = eth_apply(fn, nstack);
        nstack = 0;
        FAST_DISPATCH_NEXT();
      }

      PREDICTED(APPLYTC)
      OP(APPLYTC)
      {
        eth_t fn = r[ip->apply.fn];
        if (eth_unlikely(fn->type != eth_function_type))
        {
          while (nstack--) eth_drop(*eth_sp++);
          r[ip->apply.out] = eth_exn(eth_sym("apply_error"));
          nstack = 0;
          FAST_DISPATCH_NEXT();
        }

        eth_function *restrict func = ETH_FUNCTION(fn);
        if (func->islam && func->clos.bc->nreg <= nreg && func->arity == nstack)
        {
          bc = func->clos.bc;
          ip = func->clos.bc->code;
          eth_this = func;
          eth_nargs = nstack;
          nstack = 0;
          PREDICT(POP);
          FAST_DISPATCH();
        }
        else
        {
          r[ip->apply.out] = eth_apply(fn, nstack);
          nstack = 0;
          FAST_DISPATCH_NEXT();
        }
      }

      OP(TEST)
      {
        test = r[ip->test.vid] != eth_false;
        PREDICT_NEXT(JZE);
        FAST_DISPATCH_NEXT();
      }

      OP(TESTTY)
      {
        test = r[ip->testty.vid]->type == ip->testty.type;
        PREDICT_NEXT(JZE);
        FAST_DISPATCH_NEXT();
      }

      OP(TESTIS)
      {
        test = r[ip->testty.vid] == ip->testis.cval;
        FAST_DISPATCH_NEXT();
      }

      OP(TESTEQUAL)
      {
        test = eth_equal(r[ip->testequal.vid], ip->testequal.cval);
        FAST_DISPATCH_NEXT();
      }

      OP(GETTEST)
      {
        r[ip->gettest.out] = eth_boolean(test);
        FAST_DISPATCH_NEXT();
      }

      OP(DUP)
      {
        r[ip->dup.out] = r[ip->dup.vid];
        FAST_DISPATCH_NEXT();
      }

      OP(JNZ)
      {
        if (test)
          ip += ip->jnz.offs - 1; // `-1` to compensate for loop increment
        FAST_DISPATCH_NEXT();
      }

      PREDICTED(JZE)
      OP(JZE)
      {
        if (not test)
          ip += ip->jze.offs - 1; // `-1` to compensate for loop increment
        FAST_DISPATCH_NEXT();
      }

      OP(JMP)
      {
        ip += ip->jmp.offs;
        FAST_DISPATCH();
      }

      OP(LOOP)
      {
        uint64_t n = ip->loop.n;
        uint64_t *args = ip->loop.vids;
        for (uint64_t i = 0; i < n; ++i)
          r[i] = r[args[i]];
        ip += ip->loop.offs;
        FAST_DISPATCH();
      }

      OP(REF)
      {
        eth_ref(r[ip->ref.vid]);
        FAST_DISPATCH_NEXT();
      }

      OP(DEC)
      {
        eth_dec(r[ip->dec.vid]);
        FAST_DISPATCH_NEXT();
      }

      OP(UNREF)
      {
        eth_unref(r[ip->unref.vid]);
        FAST_DISPATCH_NEXT();
      }

      OP(DROP)
      {
        eth_drop(r[ip->drop.vid]);
        FAST_DISPATCH_NEXT();
      }

      OP(RET)
      {
        return r[ip->ret.vid];
      }

#define ARITHM_BINOP(opc, op)                                 \
      OP(opc)                                                 \
      {                                                       \
        eth_number_t lhs = ETH_NUMBER(r[ip->binop.lhs])->val; \
        eth_number_t rhs = ETH_NUMBER(r[ip->binop.rhs])->val; \
        r[ip->binop.out] = eth_num(op);                       \
        DISPATCH_NEXT();                                 \
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
      OP(opc)                                                 \
      {                                                       \
        eth_number_t lhs = ETH_NUMBER(r[ip->binop.lhs])->val; \
        eth_number_t rhs = ETH_NUMBER(r[ip->binop.rhs])->val; \
        r[ip->binop.out] = eth_boolean(op);                   \
        FAST_DISPATCH_NEXT();                                 \
      }
      ARITHM_CMP_BINOP(EQ, lhs == rhs)
      ARITHM_CMP_BINOP(NE, lhs != rhs)

#define ARITHM_CMP_BINOP2(opc, op)                            \
      OP(opc)                                                 \
      {                                                       \
        eth_number_t lhs = ETH_NUMBER(r[ip->binop.lhs])->val; \
        eth_t rhsval = r[ip->binop.rhs];                      \
        eth_number_t rhs = ETH_NUMBER(rhsval)->val;           \
        r[ip->binop.out] = op ? rhsval : eth_false;           \
        FAST_DISPATCH_NEXT();                                 \
      }
      ARITHM_CMP_BINOP2(LT, lhs < rhs)
      ARITHM_CMP_BINOP2(LE, lhs <= rhs)
      ARITHM_CMP_BINOP2(GT, lhs > rhs)
      ARITHM_CMP_BINOP2(GE, lhs >= rhs)

      OP(IS)
      {
        r[ip->binop.out] = eth_boolean(r[ip->binop.lhs] == r[ip->binop.rhs]);
        FAST_DISPATCH_NEXT();
      }

      OP(EQUAL)
      {
        r[ip->binop.out] = eth_boolean(eth_equal(r[ip->binop.lhs], r[ip->binop.rhs]));
        FAST_DISPATCH_NEXT();
      }

      OP(CONS)
      {
        r[ip->binop.out] = eth_cons_noref(r[ip->binop.lhs], r[ip->binop.rhs]);
        FAST_DISPATCH_NEXT();
      }

      OP(NOT)
      {
        r[ip->unop.out] = eth_boolean(r[ip->unop.vid] == eth_false);
        FAST_DISPATCH_NEXT();
      }

      OP(LNOT)
      {
        r[ip->unop.out] = eth_num(~(intmax_t)ETH_NUMBER(r[ip->unop.vid])->val);
        FAST_DISPATCH_NEXT();
      }

      OP(FN)
      {
        size_t ncap = ip->fn.data->ncap;
        eth_t *cap = malloc(sizeof(eth_t) * ncap);
        for (size_t i = 0; i < ncap; ++i)
          cap[i] = r[ip->fn.data->caps[i]];
        eth_t fn = eth_create_clos(ip->fn.data->src, ip->fn.data->bc, cap, ncap,
            ip->fn.data->arity);
        r[ip->fn.out] = fn;
        FAST_DISPATCH_NEXT();
      }

      OP(FINFN)
      {
        size_t ncap = ip->fn.data->ncap;
        eth_t *cap = malloc(sizeof(eth_t) * ncap);
        for (size_t i = 0; i < ncap; ++i)
          cap[i] = r[ip->fn.data->caps[i]];
        eth_function *fn = ETH_FUNCTION(r[ip->fn.out]);
        eth_finalize_clos(fn, ip->fn.data->src, ip->fn.data->bc, cap, ncap,
            ip->fn.data->arity);
        FAST_DISPATCH_NEXT();
      }

      OP(ALCFN)
      {
        r[ip->alcfn.out] = eth_create_dummy_func(ip->alcfn.arity);
        FAST_DISPATCH_NEXT();
      }

      OP(CAP)
      {
        memcpy(r + ip->cap.vid0, eth_this->clos.cap, sizeof(eth_t) * ip->cap.n);
        FAST_DISPATCH_NEXT();
      }

      OP(MKSCP)
      {
        eth_function *clos[ip->mkscp.data->nclos];
        for (size_t i = 0; i < ip->mkscp.data->nclos; ++i)
          clos[i] = ETH_FUNCTION(r[ip->mkscp.data->clos[i]]);
        eth_create_scp(clos, ip->mkscp.data->nclos);
        FAST_DISPATCH_NEXT();
      }

      OP(LOAD)
      {
        r[ip->load.out] = *(eth_t*) ((char*)r[ip->load.vid] + ip->load.offs);
        FAST_DISPATCH_NEXT();
      }

      OP(LOADRCRD)
      {
        eth_t x = r[ip->loadrcrd.src];
        eth_type *restrict type = x->type;
        test = type->flag == ETH_TFLAG_RECORD;
        if (eth_likely(test))
        {
          size_t *restrict ids = type->fieldids;
          const int n = type->nfields;
          const int N = ip->loadrcrd.n;
          int I = 0;
          size_t id = ip->loadrcrd.ids[I];
          int i;
          ids[n] = id;
          for (i = 0; true; ++i)
          {
            if (ids[i] == id)
            {
              if (eth_unlikely(i == n))
              { // not found
                test = 0;
                break;
              }
              else
              {
                r[ip->loadrcrd.vids[I++]] = eth_tup_get(x, i);
                if (I == N) break;
                id = ip->loadrcrd.ids[I];
                ids[n] = id;
              }
            }
          }
          /*if (eth_unlikely(i == n))*/
            /*test = 0;*/
        }
        FAST_DISPATCH_NEXT();
      }

      // TODO: should it realy work for any plane type (not records only)?
      OP(LOADRCRD1)
      {
        eth_t x = r[ip->loadrcrd1.vid];
        eth_type *restrict type = x->type;
        test = type->flag & ETH_TFLAG_PLAIN;
        if (eth_likely(test))
        {
          size_t *restrict ids = type->fieldids;
          const int n = type->nfields;
          const size_t id = ip->loadrcrd1.id;
          int i;
          ids[n] = id;
          for (i = 0; true; ++i)
          {
            if (ids[i] == id)
              break;
          }
          if (eth_unlikely(i == n))
            test = 0;
          else
            r[ip->loadrcrd1.out] = eth_tup_get(x, i);
        }
        FAST_DISPATCH_NEXT();
      }

      OP(SETEXN)
      {
        exn = r[ip->setexn.vid];
        FAST_DISPATCH_NEXT();
      }

      OP(GETEXN)
      {
        r[ip->getexn.out] = exn;
        FAST_DISPATCH_NEXT();
      }

      OP(MKRCRD)
      {
        eth_type *type = ip->mkrcrd.type;
        uint64_t *vids = ip->mkrcrd.vids;
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
        FAST_DISPATCH_NEXT();
      }

      OP(UPDTRCRD)
      {
        eth_t const x = r[ip->updtrcrd.src];
        eth_type *restrict const type = x->type;
        test = type->flag == ETH_TFLAG_RECORD;
        if (eth_likely(test))
        {
          size_t *restrict const ids = type->fieldids;
          const int n = type->nfields;
          assert(n > 0);
          const int N = ip->updtrcrd.n;
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
          int I = 0;
          size_t id = ip->updtrcrd.ids[I];
          int i;
          ids[n] = id;
          for (i = 0; true; ++i)
          {
            if (ids[i] == id)
            {
              if (eth_unlikely(i == n))
              { // not found
                test = 0;
                for (i = n - 1; i >= 0; --i)
                  eth_dec(rec->data[i]);
                switch (n)
                {
                  case 1:  eth_free_h1(rec); break;
                  case 2:  eth_free_h2(rec); break;
                  case 3:  eth_free_h3(rec); break;
                  case 4:  eth_free_h4(rec); break;
                  case 5:  eth_free_h5(rec); break;
                  case 6:  eth_free_h6(rec); break;
                  default: free(rec);
                }
                break;
              }
              else
              {
                eth_ref(rec->data[i] = r[ip->updtrcrd.vids[I++]]);
                if (I == N)
                {
                  for (i += 1; i < n; ++i)
                    eth_ref(rec->data[i] = eth_tup_get(x, i));
                  eth_init_header(rec, type);
                  eth_ref(ETH(rec)); // due to PHI-semantics
                  r[ip->updtrcrd.out] = ETH(rec);
                  break;
                }
                id = ip->updtrcrd.ids[I];
                ids[n] = id;
              }
            }
            else
              eth_ref(rec->data[i] = eth_tup_get(x, i));
          }
        }
        FAST_DISPATCH_NEXT();
      }
    }

    ip += 1;
  }
}

