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
    for (int i = 0; i < eth_nargs; ++i)
      eth_ref(eth_sp[i]);
    for (int i = 0; i < eth_nargs; ++i)
      eth_unref(eth_sp[i]);
    eth_pop_stack(eth_nargs);
    return eth_exn(eth_sym("Stack_overflow"));
  }

  eth_t *restrict r = alloca(nreg * sizeof(eth_t));
  bool test = 0;
  int nstack = 0;
  eth_t exn = NULL;

  register eth_bc_insn *restrict ip = bc->code;
  static void *insn[] = {
    [ETH_OPC_CVAL   ] = &&insn_cval,
    [ETH_OPC_PUSH   ] = &&insn_push,
    [ETH_OPC_POP    ] = &&insn_pop,
    [ETH_OPC_APPLY  ] = &&insn_apply,
    [ETH_OPC_APPLYTC] = &&insn_applytc,
    [ETH_OPC_TEST   ] = &&insn_test,
    [ETH_OPC_TESTTY ] = &&insn_testty,
    [ETH_OPC_TESTIS ] = &&insn_testis,
    [ETH_OPC_GETTEST] = &&insn_gettest,
    [ETH_OPC_DUP    ] = &&insn_dup,
    [ETH_OPC_JNZ    ] = &&insn_jnz,
    [ETH_OPC_JZE    ] = &&insn_jze,
    [ETH_OPC_JMP    ] = &&insn_jmp,
    [ETH_OPC_REF    ] = &&insn_ref,
    [ETH_OPC_DEC    ] = &&insn_dec,
    [ETH_OPC_UNREF  ] = &&insn_unref,
    [ETH_OPC_DROP   ] = &&insn_drop,
    [ETH_OPC_RET    ] = &&insn_ret,
    [ETH_OPC_ADD    ] = &&insn_add,
    [ETH_OPC_SUB    ] = &&insn_sub,
    [ETH_OPC_MUL    ] = &&insn_mul,
    [ETH_OPC_DIV    ] = &&insn_div,
    [ETH_OPC_MOD    ] = &&insn_mod,
    [ETH_OPC_POW    ] = &&insn_pow,
    [ETH_OPC_LAND   ] = &&insn_land,
    [ETH_OPC_LOR    ] = &&insn_lor,
    [ETH_OPC_LXOR   ] = &&insn_lxor,
    [ETH_OPC_LSHL   ] = &&insn_lshl,
    [ETH_OPC_LSHR   ] = &&insn_lshr,
    [ETH_OPC_ASHL   ] = &&insn_ashl,
    [ETH_OPC_ASHR   ] = &&insn_ashr,
    [ETH_OPC_LT     ] = &&insn_lt,
    [ETH_OPC_LE     ] = &&insn_le,
    [ETH_OPC_GT     ] = &&insn_gt,
    [ETH_OPC_GE     ] = &&insn_ge,
    [ETH_OPC_EQ     ] = &&insn_eq,
    [ETH_OPC_NE     ] = &&insn_ne,
    [ETH_OPC_IS     ] = &&insn_is,
    [ETH_OPC_EQUAL  ] = &&insn_equal,
    [ETH_OPC_CONS   ] = &&insn_cons,
    [ETH_OPC_NOT    ] = &&insn_not,
    [ETH_OPC_LNOT   ] = &&insn_lnot,
    [ETH_OPC_FN] = &&insn_fn,
    [ETH_OPC_FINFN] = &&insn_finfn,
    [ETH_OPC_ALCFN] = &&insn_alcfn,
    [ETH_OPC_CAP] = &&insn_cap,
    [ETH_OPC_MKSCP] = &&insn_mkscp,
    [ETH_OPC_LOAD] = &&insn_load,
    [ETH_OPC_LOADRCRD] = &&insn_loadrcrd,
    [ETH_OPC_LOADRCRD1] = &&insn_loadrcrd1,
    [ETH_OPC_SETEXN] = &&insn_setexn,
    [ETH_OPC_GETEXN] = &&insn_getexn,
    [ETH_OPC_MKRCRD] = &&insn_mkrcrd,
  };
#define DISPATCH_NEXT() goto *insn[(++ip)->opc]
#define DISPATCH() goto *insn[ip->opc]

  DISPATCH();

  insn_cval:
  {
    r[ip->cval.out] = ip->cval.val;
    DISPATCH_NEXT();
  }

  insn_push:
  {
    eth_reserve_stack(ip->push.n);
    for (size_t i = 0; i < ip->push.n; ++i)
      eth_sp[i] = r[ip->push.vids[i]];
    nstack += ip->push.n;
    DISPATCH_NEXT();
  }

  insn_pop:
  {
    memcpy(r + ip->pop.vid0, eth_sp, sizeof(eth_t) * ip->pop.n);
    eth_pop_stack(ip->pop.n);
    DISPATCH_NEXT();
  }

  insn_apply:
  {
    eth_t fn = r[ip->apply.fn];
    if (eth_unlikely(fn->type != eth_function_type))
    {
      while (nstack--) eth_drop(*eth_sp++);
      r[ip->apply.out] = eth_exn(eth_sym("Apply_error"));
      nstack = 0;
      DISPATCH_NEXT();
    }
    r[ip->apply.out] = eth_apply(fn, nstack);
    nstack = 0;
    DISPATCH_NEXT();
  }

  insn_applytc:
  {
    eth_t fn = r[ip->apply.fn];
    if (eth_unlikely(fn->type != eth_function_type))
    {
      while (nstack--) eth_drop(*eth_sp++);
      r[ip->apply.out] = eth_exn(eth_sym("Apply_error"));
      nstack = 0;
      DISPATCH_NEXT();
    }

    eth_function *restrict func = ETH_FUNCTION(fn);
    if (func->islam && func->clos.bc->nreg <= nreg && func->arity == nstack)
    {
      bc = func->clos.bc;
      ip = func->clos.bc->code;
      eth_this = func;
      eth_nargs = nstack;
      nstack = 0;
      DISPATCH();
    }
    else
    {
      r[ip->apply.out] = eth_apply(fn, nstack);
      nstack = 0;
      DISPATCH_NEXT();
    }
  }

  insn_test:
  {
    test = r[ip->test.vid] != eth_false;
    DISPATCH_NEXT();
  }

  insn_testty:
  {
    test = r[ip->testty.vid]->type == ip->testty.type;
    DISPATCH_NEXT();
  }

  insn_testis:
  {
    test = r[ip->testty.vid] == ip->testis.cval;
    DISPATCH_NEXT();
  }

  insn_gettest:
  {
    r[ip->gettest.out] = eth_boolean(test);
    DISPATCH_NEXT();
  }

  insn_dup:
  {
    r[ip->dup.out] = r[ip->dup.vid];
    DISPATCH_NEXT();
  }

  insn_jnz:
  {
    if (test)
      ip += ip->jnz.offs - 1; // `-1` to compensate for loop increment
    DISPATCH_NEXT();
  }

  insn_jze:
  {
    if (not test)
      ip += ip->jze.offs - 1; // `-1` to compensate for loop increment
    DISPATCH_NEXT();
  }

  insn_jmp:
  {
    ip += ip->jmp.offs - 1; // `-1` to compensate for loop increment
    DISPATCH_NEXT();
  }

  insn_ref:
  {
    eth_ref(r[ip->ref.vid]);
    DISPATCH_NEXT();
  }

  insn_dec:
  {
    eth_dec(r[ip->dec.vid]);
    DISPATCH_NEXT();
  }

  insn_unref:
  {
    eth_unref(r[ip->unref.vid]);
    DISPATCH_NEXT();
  }

  insn_drop:
  {
    eth_drop(r[ip->drop.vid]);
    DISPATCH_NEXT();
  }

  insn_ret:
  {
    return r[ip->ret.vid];
  }

#define ARITHM_BINOP(opc, op)                             \
  insn_##opc:                                             \
  {                                                       \
    eth_number_t lhs = ETH_NUMBER(r[ip->binop.lhs])->val; \
    eth_number_t rhs = ETH_NUMBER(r[ip->binop.rhs])->val; \
    r[ip->binop.out] = eth_num(op);                       \
    DISPATCH_NEXT();                                      \
  }
  ARITHM_BINOP(add, lhs + rhs)
  ARITHM_BINOP(sub, lhs - rhs)
  ARITHM_BINOP(mul, lhs * rhs)
  ARITHM_BINOP(div, lhs / rhs)
  ARITHM_BINOP(mod, eth_mod(lhs, rhs))
  ARITHM_BINOP(pow, eth_pow(lhs, rhs))
  // ---
  ARITHM_BINOP(land, (intmax_t)lhs & (intmax_t)rhs)
  ARITHM_BINOP(lor,  (intmax_t)lhs | (intmax_t)rhs)
  ARITHM_BINOP(lxor, (intmax_t)lhs ^ (intmax_t)rhs)
  ARITHM_BINOP(lshl, (uintmax_t)lhs << (uintmax_t)rhs)
  ARITHM_BINOP(lshr, (uintmax_t)lhs >> (uintmax_t)rhs)
  ARITHM_BINOP(ashl, (intmax_t)lhs << (intmax_t)rhs)
  ARITHM_BINOP(ashr, (intmax_t)lhs >> (intmax_t)rhs)

#define ARITHM_CMP_BINOP(opc, op)                         \
  insn_##opc:                                             \
  {                                                       \
    eth_number_t lhs = ETH_NUMBER(r[ip->binop.lhs])->val; \
    eth_number_t rhs = ETH_NUMBER(r[ip->binop.rhs])->val; \
    r[ip->binop.out] = eth_boolean(op);                   \
    DISPATCH_NEXT();                                      \
  }
  ARITHM_CMP_BINOP(lt, lhs < rhs)
  ARITHM_CMP_BINOP(le, lhs <= rhs)
  ARITHM_CMP_BINOP(gt, lhs > rhs)
  ARITHM_CMP_BINOP(ge, lhs >= rhs)
  ARITHM_CMP_BINOP(eq, lhs == rhs)
  ARITHM_CMP_BINOP(ne, lhs != rhs)

  insn_is:
  {
    r[ip->binop.out] = eth_boolean(r[ip->binop.lhs] == r[ip->binop.rhs]);
    DISPATCH_NEXT();
  }

  insn_equal:
  {
    r[ip->binop.out] = eth_boolean(eth_equal(r[ip->binop.lhs], r[ip->binop.rhs]));
    DISPATCH_NEXT();
  }

  insn_cons:
  {
    r[ip->binop.out] = eth_cons(r[ip->binop.lhs], r[ip->binop.rhs]);
    DISPATCH_NEXT();
  }

  insn_not:
  {
    r[ip->unop.out] = eth_boolean(r[ip->unop.vid] == eth_false);
    DISPATCH_NEXT();
  }

  insn_lnot:
  {
    r[ip->unop.out] = eth_num(~(intmax_t)ETH_NUMBER(r[ip->unop.vid])->val);
    DISPATCH_NEXT();
  }

  insn_fn:
  {
    size_t ncap = ip->fn.data->ncap;
    eth_t *cap = malloc(sizeof(eth_t) * ncap);
    for (size_t i = 0; i < ncap; ++i)
      cap[i] = r[ip->fn.data->caps[i]];
    eth_t fn = eth_create_clos(ip->fn.data->ir, ip->fn.data->bc, cap, ncap,
        ip->fn.data->arity);
    r[ip->fn.out] = fn;
    DISPATCH_NEXT();
  }

  insn_finfn:
  {
    size_t ncap = ip->fn.data->ncap;
    eth_t *cap = malloc(sizeof(eth_t) * ncap);
    for (size_t i = 0; i < ncap; ++i)
      cap[i] = r[ip->fn.data->caps[i]];
    eth_function *fn = ETH_FUNCTION(r[ip->fn.out]);
    eth_finalize_clos(fn, ip->fn.data->ir, ip->fn.data->bc, cap, ncap,
        ip->fn.data->arity);
    DISPATCH_NEXT();
  }

  insn_alcfn:
  {
    r[ip->alcfn.out] = eth_create_dummy_func(ip->alcfn.arity);
    DISPATCH_NEXT();
  }

  insn_cap:
  {
    memcpy(r + ip->cap.vid0, eth_this->clos.cap, sizeof(eth_t) * ip->cap.n);
    DISPATCH_NEXT();
  }

  insn_mkscp:
  {
    eth_t wrefs[ip->mkscp.data->nwref];
    eth_function *clos[ip->mkscp.data->nclos];
    for (size_t i = 0; i < ip->mkscp.data->nwref; ++i)
      wrefs[i] = r[ip->mkscp.data->wrefs[i]];
    for (size_t i = 0; i < ip->mkscp.data->nclos; ++i)
      clos[i] = ETH_FUNCTION(r[ip->mkscp.data->clos[i]]);
    eth_create_scp(clos, ip->mkscp.data->nclos, wrefs, ip->mkscp.data->nwref);
    DISPATCH_NEXT();
  }

  insn_load:
  {
    r[ip->load.out] = *(eth_t*) ((char*)r[ip->load.vid] + ip->load.offs);
    DISPATCH_NEXT();
  }

  insn_loadrcrd:
  {
    eth_t x = r[ip->loadrcrd.src];
    eth_type *restrict type = x->type;
    test = type->flag & ETH_TFLAG_PLAIN;
    if (eth_likely(test))
    {
      size_t *restrict ids = type->fieldids;
      const int n = type->nfields;
      const int N = ip->loadrcrd.n;
      int I = 0;
      size_t id = ip->loadrcrd.ids[I];
      int i;
      for (i = 0; i < n; ++i)
      {
        if (ids[i] == id)
        {
          r[ip->loadrcrd.vids[I++]] = eth_tup_get(x, i);
          if (I == N) break;
          id = ip->loadrcrd.ids[I];
        }
      }
      if (eth_unlikely(i == n))
        test = 0;
    }
    DISPATCH_NEXT();
  }

  insn_loadrcrd1:
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
    DISPATCH_NEXT();
  }

  insn_setexn:
  {
    exn = r[ip->setexn.vid];
    DISPATCH_NEXT();
  }

  insn_getexn:
  {
    eth_t what = ETH_EXCEPTION(exn)->what;
    eth_ref(what);
    eth_unref(exn);
    r[ip->getexn.out] = what;
    DISPATCH_NEXT();
  }

  insn_mkrcrd:
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
    DISPATCH_NEXT();
  }
}

