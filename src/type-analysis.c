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


ETH_MODULE("ether:type-analysis")


static eth_atype*
create_atype(eth_atype_tag tag)
{
  eth_atype *aty = malloc(sizeof(eth_atype));
  aty->tag = tag;
  aty->rc = 0;
  return aty;
}

static void
destroy_atype(eth_atype *aty)
{
  switch (aty->tag)
  {
    case ETH_ATYPE_ERROR:
      break;

    case ETH_ATYPE_UNION:
      for (int i = 0; i < aty->un.n; ++i)
        eth_unref_atype(aty->un.types[i]);
      free(aty->un.types);
      break;

    case ETH_ATYPE_RECORD:
      for (int i = 0; i < aty->record.n; ++i)
        eth_unref_atype(aty->record.types[i]);
      free(aty->record.ids);
      free(aty->record.types);
      break;

    case ETH_ATYPE_TUPLE:
      for (int i = 0; i < aty->tuple.n; ++i)
        eth_unref_atype(aty->tuple.types[i]);
      free(aty->tuple.types);
      break;

    case ETH_ATYPE_LIST:
      eth_unref_atype(aty->list.elttype);
      break;

    case ETH_ATYPE_FN:
      eth_unref_ir(aty->fn.body);
      break;

    case ETH_ATYPE_ATOM:
      break;
  }

  free(aty);
}

void
eth_ref_atype(eth_atype *aty)
{
  aty->rc += 1;
}

void
eth_unref_atype(eth_atype *aty)
{
  if (--aty->rc == 0)
    destroy_atype(aty);
}

void
eth_drop_atype(eth_atype *aty)
{
  if (aty->rc == 0)
    destroy_atype(aty);
}

eth_atype*
eth_create_atype_error(void)
{
  return create_atype(ETH_ATYPE_ERROR);
}

eth_atype*
eth_create_atype_atom(eth_type *type)
{
  eth_atype *aty = create_atype(ETH_ATYPE_ATOM);
  aty->atom.type = type;
  return aty;
}

eth_atype*
eth_create_atype_list(eth_atype *elttype)
{
  eth_atype *aty = create_atype(ETH_ATYPE_LIST);
  eth_ref_atype(aty->list.elttype = elttype);
  return aty;
}

eth_atype*
eth_create_atype_record(int const ids[], eth_atype *const types[], int n)
{
  eth_atype *aty = create_atype(ETH_ATYPE_RECORD);
  aty->record.ids = malloc(sizeof(int) * n);
  aty->record.types = malloc(sizeof(eth_atype*) * n);
  aty->record.n = n;
  for (int i = 0; i < n; ++i)
  {
    aty->record.ids[i] = ids[i];
    eth_ref_atype(aty->record.types[i] = types[i]);
  }
  return aty;
}

eth_atype*
eth_create_atype_tuple(eth_atype *const types[], int n)
{
  eth_atype *aty = create_atype(ETH_ATYPE_TUPLE);
  aty->tuple.types = malloc(sizeof(eth_atype*) * n);
  aty->tuple.n = n;
  for (int i = 0; i < n; ++i)
    eth_ref_atype(aty->tuple.types[i] = types[i]);
  return aty;
}

eth_atype*
eth_create_atype_fn(int arity, eth_ir *body)
{
  eth_atype *aty = create_atype(ETH_ATYPE_FN);
  aty->fn.arity = arity;
  eth_ref_ir(aty->fn.body = body);
  return aty;
}

typedef cod_vec(eth_atype*) atype_vec;

// TODO: don't duplicate (but do I realy care?)
static void
union_types(eth_atype *const types[], int n, atype_vec *vec)
{
  for (int i = 0; i < n; ++i)
  {
    if (types[i]->tag == ETH_ATYPE_UNION)
    {
      union_types(types[i]->un.types, types[i]->un.n, vec);
    }
    else
    {
      cod_vec_push(*vec, types[i]);
      eth_ref_atype(types[i]);
    }
  }
}

eth_atype*
eth_create_atype_union(eth_atype *const types[], int n)
{
  eth_atype *aty = create_atype(ETH_ATYPE_UNION);
  atype_vec buf;
  cod_vec_init(buf);
  union_types(types, n, &buf);
  aty->un.types = buf.data;
  aty->un.n = buf.len;
  return aty;
}

typedef struct var_info {
  eth_atype *atype;
  eth_t cval;
} var_info;

typedef struct context {
  var_info *vinfo;
  int nvars;
} context;

static void
init_context(context *ctx, const eth_ir *ir)
{
  ctx->nvars = ir->nvars;
  ctx->vinfo = calloc(sizeof(var_info), ir->nvars);
}

static void
destroy_context(context *ctx)
{
  for (int i = 0; i < ctx->nvars; ++i)
  {
    if (ctx->vinfo[i].atype)
    {
      eth_unref_atype(ctx->vinfo[i].atype);
      if (ctx->vinfo[i].cval)
        eth_unref(ctx->vinfo[i].cval);
    }
  }
  free(ctx->vinfo);
}

static inline void
set_var_atype(context *ctx, int vid, eth_atype *atype)
{
  assert(ctx->vinfo[vid].atype == NULL);
  eth_ref_atype(ctx->vinfo[vid].atype = atype);
}

static inline eth_atype*
get_var_atype(context *ctx, int vid)
{
  assert(ctx->vinfo[vid].atype != NULL);
  return ctx->vinfo[vid].atype;
}

static inline void
set_var_cval(context *ctx, int vid, eth_t val)
{
  assert(ctx->vinfo[vid].cval == NULL);
  eth_ref(ctx->vinfo[vid].cval = val);
}

static inline eth_t
get_var_cval(context *ctx, int vid)
{
  assert(ctx->vinfo[vid].cval != NULL);
  return ctx->vinfo[vid].cval;
}

static inline bool
var_has_cval(context *ctx, int vid)
{
  return ctx->vinfo[vid].cval != NULL;
}

typedef struct {
  eth_atype *ret;
  eth_ir_node *ir;
} ir_spec;

static inline void
destroy_ir_spec(ir_spec *spec)
{
  eth_unref_atype(spec->ret);
  eth_unref_ir_node(spec->ir);
}

// TODO: don't loose locations
static void
specialize(context *ctx, eth_ir_node *ir, ir_spec *out, bool *err)
{
#define RETURN(_ret, _ir)           \
  do {                              \
    eth_ref_atype(out->ret = _ret); \
    eth_ref_ir_node(out->ir = _ir); \
    return;                         \
  } while (0)
#define OK true
#define ERR false

  switch (ir->tag)
  {
    case ETH_IR_ERROR:
    {
      eth_error("ERROR-node passed to IR-analyser");
      abort();
    }

    case ETH_IR_CVAL:
    {
      eth_atype *ret = eth_create_atype_atom(ir->cval.val->type);
      RETURN(ret, ir);
    }

    case ETH_IR_VAR:
    {
      int vid = ir->var.vid;
      eth_ir_node *retir;
      if (var_has_cval(ctx, vid))
        retir = eth_ir_cval(get_var_cval(ctx, vid));
      else
        retir = ir;
      RETURN(get_var_atype(ctx, vid), ir);
    }

    case ETH_IR_APPLY:
    {
      eth_unimplemented();
    }

    case ETH_IR_IF:
    {
      ir_spec condspec;
      specialize(ctx, ir->iff.cond, &condspec, err);
      // --
      if (condspec.ir->tag == ETH_IR_CVAL)
      {
        if (condspec.ir->cval.val == eth_false)
        {
          if (ir->iff.toplvl == ETH_TOPLVL_ELSE)
          {
            eth_error("specialization would remove toplevel branch");
            *err = true;
            destroy_ir_spec(&condspec);
            RETURN(eth_create_atype_error(), ir);
          }
          specialize(ctx, ir->iff.elsebr, out, err);
        }
        else
        {
          if (ir->iff.toplvl == ETH_TOPLVL_THEN)
          {
            eth_error("specialization would remove toplevel branch");
            *err = true;
            destroy_ir_spec(&condspec);
            RETURN(eth_create_atype_error(), ir);
          }
          specialize(ctx, ir->iff.thenbr, out, err);
        }
        // --
        destroy_ir_spec(&condspec);
        return;
      }
      else
      {
        ir_spec thenspec, elsespec;
        specialize(ctx, ir->iff.thenbr, &thenspec, err);
        specialize(ctx, ir->iff.elsebr, &elsespec, err);
        // --
        // TODO: optimize (maybe same types)
        eth_atype *rets[] = { thenspec.ret, elsespec.ret };
        eth_atype *ret = eth_create_atype_union(rets, 2);
        eth_ir_node *ir = eth_ir_if(condspec.ir, thenspec.ir, elsespec.ir);
        ir->iff.toplvl = ir->iff.toplvl;
        // --
        destroy_ir_spec(&condspec);
        destroy_ir_spec(&thenspec);
        destroy_ir_spec(&elsespec);
        RETURN(ret, ir);
      }
    }

    case ETH_IR_TRY:
    {
      eth_unimplemented();
    }

    case ETH_IR_SEQ:
    {
      ir_spec e1spec, e2spec;
      // --
      specialize(ctx, ir->seq.e1, &e1spec, err);
      if (not eth_atype_is_atom(e1spec.ret, eth_nil_type))
      {
        eth_warning("non-nil-valued intermediate expression");
        *err = true;
        RETURN(eth_create_atype_error(), ir);
      }
      // --
      specialize(ctx, ir->seq.e2, &e2spec, err);
      // --
      eth_ref_atype(out->ret = e2spec.ret);
      out->ir = eth_ir_seq(e1spec.ir, e2spec.ir);
      destroy_ir_spec(&e1spec);
      destroy_ir_spec(&e2spec);
      return;
    }

    case ETH_IR_BINOP:
    {
      eth_unimplemented();
    }

    case ETH_IR_UNOP:
    {
      eth_unimplemented();
    }

    // TODO: captures may change after specialization
    case ETH_IR_FN:
    {
      context fnctx;
      init_context(&fnctx, ir->fn.body);
      // init arguments
      for (int i = 0; i < ir->fn.arity; ++i)
        set_var_atype(&fnctx, i, eth_create_atype_any());
      // set up captures
      for (int i = 0; i < ir->fn.ncap; ++i)
        set_var_atype(&fnctx, ir->fn.capvars[i], get_var_atype(ctx, ir->fn.caps[i]));
      // body
      ir_spec fnspec;
      specialize(&fnctx, ir->fn.body->body, &fnspec, err);
      eth_ir *newbody = eth_create_ir(fnspec.ir, fnctx.nvars);
      eth_atype *ret = eth_create_atype_fn(ir->fn.arity, newbody);
      eth_ir_node *newir = eth_ir_fn(ir->fn.arity, ir->fn.caps, ir->fn.capvars,
          ir->fn.ncap, newbody, ir->fn.ast);
      destroy_context(&fnctx);
      destroy_ir_spec(&fnspec);
      RETURN(ret, newir);
    }

    case ETH_IR_MATCH:
    {
      eth_unimplemented();
    }

    case ETH_IR_MULTIMATCH:
    {
      eth_unimplemented();
    }

    case ETH_IR_STARTFIX:
    {
      eth_unimplemented();
    }

    case ETH_IR_ENDFIX:
    {
      eth_unimplemented();
    }

    case ETH_IR_MKRCRD:
    {
      eth_unimplemented();
    }

    case ETH_IR_UPDATE:
    {
      eth_unimplemented();
    }

    case ETH_IR_THROW:
    {
      eth_unimplemented();
    }

    case ETH_IR_RETURN:
    {
      eth_unimplemented();
    }

  }
}

eth_ir_node*
eth_specialize(const eth_ir *ir)
{
  context ctx;
  ir_spec spec;
  bool err = false;

  init_context(&ctx, ir);
  specialize(&ctx, ir->body, &spec, &err);
  destroy_context(&ctx);

  if (err)
  {
    destroy_ir_spec(&spec);
    return NULL;
  }
  else
  {
    eth_unref_atype(spec.ret);
    return spec.ir;
  }
}
