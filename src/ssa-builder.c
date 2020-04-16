#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


ETH_MODULE("ether:ssa-builder")


/*
 * 1. Evaluate LETREC as is but creating dummy place-holders instead of all
 *    internal closures.
 *
 * 2. Finalize all closures.
 */
typedef struct closure closure;
typedef struct weak_ref weak_ref;

struct closure {
  int vid;
  eth_ir_node *ctor;
};

struct weak_ref {
  int vid;
};

typedef struct {
  cod_vec(weak_ref) wrefs;
  cod_vec(closure) clos;
} rec_scope;

static rec_scope*
create_rec_scope(void)
{
  rec_scope *scp = malloc(sizeof(rec_scope));
  cod_vec_init(scp->wrefs);
  cod_vec_init(scp->clos);
  return scp;
}

static void
destroy_rec_scope(rec_scope *scp)
{
  cod_vec_destroy(scp->wrefs);
  cod_vec_destroy(scp->clos);
  free(scp);
}

static int
find_weak_ref_idx(rec_scope *scp, int vid)
{
  cod_vec_iter(scp->wrefs, i, x, if (x.vid == vid) return i);
  return -1;
}


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                               BUILDER
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef enum {
  RC_RULES_DEFAULT,
  RC_RULES_DISABLE,
  RC_RULES_PHI,
  RC_RULES_UNREF,
} rc_rules;

typedef struct value_info {
  rc_rules rules;
  eth_t cval;
  eth_type *type;
  eth_insn *creatloc;
  bool isrec;
  bool isdummy;
} value_info;

typedef enum {
  ACTION_SETCVAL,
  ACTION_SETTYPE,
} action_tag;

typedef struct {
  action_tag tag;
  union {
    struct { int vid; eth_t new, old; } setcval;
    struct { int vid; eth_type *new, *old; } settype;
  };
} action;

typedef struct  {
  int nvars;
  int *vars;
  // TODO: use cod_vec
  int vilen;
  int vicap;
  value_info **vinfo;
  cod_vec(eth_insn*) movs;
  rec_scope *recscp;
  int recscpcnt;
  bool testexn;
  int ntries;
  int istry, tryid;
  cod_vec(action) actions;
} ssa_builder;

static value_info*
create_empty_vinfo()
{
  value_info *vinfo = calloc(1, sizeof(value_info));
  vinfo->isrec = false;
  return vinfo;
}

static void
destroy_vinfo(value_info *vinfo)
{
  free(vinfo);
}

static ssa_builder*
create_ssa_builder(int nvars)
{
  ssa_builder *bldr = malloc(sizeof(ssa_builder));
  bldr->nvars = nvars;
  bldr->vars = malloc(sizeof(int) * nvars);
  bldr->vilen = 0;
  bldr->vicap = 0x400;
  bldr->vinfo = malloc(sizeof(value_info*) * bldr->vicap);
  cod_vec_init(bldr->movs);
  bldr->recscp = NULL;
  bldr->recscpcnt = 0;
  bldr->testexn = true;
  bldr->ntries = 0;
  bldr->istry = 0;
  cod_vec_init(bldr->actions);
  return bldr;
}

static void
destroy_ssa_builder(ssa_builder *bldr)
{
  free(bldr->vars);
  for (int i = 0; i < bldr->vilen; ++i)
    destroy_vinfo(bldr->vinfo[i]);
  free(bldr->vinfo);
  cod_vec_destroy(bldr->movs);
  assert(bldr->recscp == NULL);
  assert(bldr->actions.len == 0);
  cod_vec_destroy(bldr->actions);
  free(bldr);
}

static int
begin_logical_block(ssa_builder *bldr)
{
  return bldr->actions.len;
}

static void
set_cval(ssa_builder *bldr, int vid, eth_t val)
{
  cod_vec_push(bldr->actions, (action) {
    .tag = ACTION_SETCVAL,
    .setcval = {
      .vid = vid,
      .new = val,
      .old = bldr->vinfo[vid]->cval,
    }
  });
  bldr->vinfo[vid]->cval = val;
}

static void
set_type(ssa_builder *bldr, int vid, eth_type *type)
{
  cod_vec_push(bldr->actions, (action) {
    .tag = ACTION_SETTYPE,
    .settype = {
      .vid = vid,
      .new = type,
      .old = bldr->vinfo[vid]->type,
    }
  });
  bldr->vinfo[vid]->type = type;
}

static void
undo_action(ssa_builder *bldr, action *a)
{
  switch (a->tag)
  {
    case ACTION_SETCVAL:
      bldr->vinfo[a->setcval.vid]->cval = a->setcval.old;
      break;

    case ACTION_SETTYPE:
      bldr->vinfo[a->settype.vid]->type = a->settype.old;
      break;
  }
}

static void
end_logical_block(ssa_builder *bldr, int start)
{
  int n = bldr->actions.len - start;
  while (n--)
  {
    action a = cod_vec_pop(bldr->actions);
    undo_action(bldr, &a);
  }
}

static int
new_try(ssa_builder *bldr)
{
  return bldr->ntries++;
}

/*
 * Create new SSA-value.
 */
static int
new_val(ssa_builder *bldr, rc_rules rules)
{
  if (bldr->vicap == bldr->vilen)
  {
    bldr->vicap <<= 1;
    bldr->vinfo = realloc(bldr->vinfo, sizeof(value_info*) * bldr->vicap);
  }
  int vid = bldr->vilen++;
  bldr->vinfo[vid] = create_empty_vinfo();
  bldr->vinfo[vid]->rules = rules;
  return vid;
}

static void
trace_mov(ssa_builder *bldr, eth_insn *insn)
{
  cod_vec_push(bldr->movs, insn);
}


static eth_ssa*
create_ssa(eth_insn *body, int nvals, int ntries)
{
  eth_ssa *ssa = malloc(sizeof(eth_ssa));
  ssa->nvals = nvals;
  ssa->ntries = ntries;
  ssa->body = body;
  return ssa;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               build SSA
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_ssa*
build_body(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *body, bool *e);

#define FN_NEW (-1)
static int
build_fn(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *fn, int f, bool *e);

static int
build(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *ir, int istc, bool *e);

static int
build_logical_block(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *ir,
    int istc, bool *e)
{
  int lbstart = begin_logical_block(bldr);
  int ret = build(bldr, tape, ir, istc, e);
  end_logical_block(bldr, lbstart);
  return ret;
}

static rec_scope*
start_rec_scope(ssa_builder *bldr)
{
  if (bldr->recscpcnt++)
    return bldr->recscp;
  else
    return bldr->recscp = create_rec_scope();
}

static void
deffer_closure(ssa_builder *bldr, int vid, eth_ir_node *ctor)
{
  assert(bldr->recscp);
  closure c = { .vid = vid, .ctor = ctor };
  cod_vec_push(bldr->recscp->clos, c);
}

static void
add_rec_variable(ssa_builder *bldr, int vid)
{
  assert(bldr->recscp);
  weak_ref wref = { .vid = vid };
  cod_vec_push(bldr->recscp->wrefs, wref);
}

static void
finalize_rec_scope(ssa_builder *bldr, eth_ssa_tape *tape, bool *e)
{
  rec_scope *scp = bldr->recscp;

  assert(bldr->recscpcnt > 0);
  if (--bldr->recscpcnt > 0)
    return;

  // finalize closures
  cod_vec_iter(scp->clos, i, x, build_fn(bldr, tape, x.ctor, x.vid, e));

  // remove marks from recursive variables
  cod_vec_iter(scp->wrefs, i, x, bldr->vinfo[x.vid]->isrec = false);

  // build scope
  int nwref = scp->wrefs.len;
  int wrefs[nwref];
  int nclos = scp->clos.len;
  int clos[nclos];
  for (int i = 0; i < nwref; ++i)
    wrefs[i] = scp->wrefs.data[i].vid;
  for (int i = 0; i < nclos; ++i)
    clos[i] = scp->clos.data[i].vid;
  eth_write_insn(tape, eth_insn_mkscp(clos, nclos, wrefs, nwref));

  destroy_rec_scope(scp);
  bldr->recscp = NULL;
}

static void
move_to_phi_default(ssa_builder *bldr, eth_ssa_tape *tape, int phi, int vid)
{
  switch (bldr->vinfo[vid]->rules)
  {
    case RC_RULES_DEFAULT:
    case RC_RULES_PHI:
    case RC_RULES_UNREF:
    {
      eth_insn *mov = eth_insn_mov(phi, vid);
      eth_write_insn(tape, mov);
      trace_mov(bldr, mov);
      break;
    }

    case RC_RULES_DISABLE:
    {
      eth_insn *mov = eth_insn_mov(phi, vid);
      eth_write_insn(tape, mov);
      trace_mov(bldr, mov);
      break;
    }
  }
}

static int
resolve_phi(ssa_builder *bldr, eth_ssa_tape *t1, int r1, eth_ssa_tape *t2, int r2)
{
  int rc1 = bldr->vinfo[r1]->rules;
  int rc2 = bldr->vinfo[r2]->rules;
  int phi;
  if (rc1 == rc2 && rc1 == RC_RULES_DISABLE)
  {
    phi = new_val(bldr, RC_RULES_DISABLE);
    eth_insn *mov1 = eth_insn_mov(phi, r1);
    eth_insn *mov2 = eth_insn_mov(phi, r2);
    eth_write_insn(t1, mov1);
    eth_write_insn(t2, mov2);
    // don't tace these movs
  }
  else
  {
    phi = new_val(bldr, RC_RULES_PHI);
    move_to_phi_default(bldr, t1, phi, r1);
    move_to_phi_default(bldr, t2, phi, r2);
  }
  return phi;
}

static int
build_fn(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *ir, int f, bool *e)
{
  int arity = ir->fn.arity;

  // enclose outer variables:
  int ncap = ir->fn.ncap;
  int caps[ncap];
  int nmov = 0;
  int movs[ncap];
  for (int i = 0; i < ncap; ++i)
  {
    caps[i] = bldr->vars[ir->fn.caps[i]];
    if (not bldr->vinfo[caps[i]]->isrec)
      movs[nmov++] = caps[i];
  }

  ssa_builder *fnbldr = create_ssa_builder(ir->fn.body->nvars);
  eth_ssa_tape *fntape = eth_create_ssa_tape();

  // declare arguments [0, arity):
  if (arity)
  {
    int fnargs[arity];
    for (int i = 0; i < arity; ++i)
    {
      fnargs[i] = new_val(fnbldr, RC_RULES_DEFAULT);
      fnbldr->vars[i] = fnargs[i]; // varids of args are [0 ... arity)
    }

    eth_insn *pop = eth_insn_pop(fnargs, arity);
    eth_write_insn(fntape, pop);

    for (int i = 0; i < arity; ++i)
      fnbldr->vinfo[fnargs[i]]->creatloc = pop;
  }

  // declare captures [arity, arity + ncap):
  if (ncap > 0)
  {
    int fncaps[ncap];
    for (int i = 0; i < ncap; ++i)
    {
      fncaps[i] = new_val(fnbldr, RC_RULES_DISABLE);
      fnbldr->vars[ir->fn.capvars[i]] = fncaps[i];
      fnbldr->vinfo[fncaps[i]]->cval = bldr->vinfo[caps[i]]->cval;
      fnbldr->vinfo[fncaps[i]]->type = bldr->vinfo[caps[i]]->type;
    }
    eth_write_insn(fntape, eth_insn_cap(fncaps, ncap));
  }

  // build body:
  eth_ssa *ssa = build_body(fnbldr, fntape, ir->fn.body->body, e);
  eth_destroy_ssa_tape(fntape);
  destroy_ssa_builder(fnbldr);

  int ret;
  eth_insn *insn;
  if (f == FN_NEW)
  {
    assert(nmov == ncap);
    ret = new_val(bldr, RC_RULES_DEFAULT);
    insn = eth_insn_fn(ret, arity, caps, ncap, ir->fn.body, ssa);
    bldr->vinfo[ret]->creatloc = insn;
  }
  else
  {
    ret = -1;
    insn = eth_insn_finfn(f, arity, caps, ncap, movs, nmov, ir->fn.body, ssa);
  }
  eth_write_insn(tape, insn);
  trace_mov(bldr, insn);
  return ret;
}

static eth_t
create_tracer(eth_location *loc)
{
  eth_t proc(void)
  {
    eth_location *loc = eth_this->proc.data;
    eth_t exn = *eth_sp++;
    assert(exn->type == eth_exception_type);
    if (exn->rc > 0)
      exn = eth_copy_exception(exn);
    eth_push_trace(exn, loc);
    return exn;
  }
  eth_t tracer = eth_create_proc(proc, 1, loc, (void*)eth_unref_location);
  eth_ref_location(loc);
  return tracer;
}

static void
write_throw(ssa_builder *bldr, eth_ssa_tape *tape, int exnvid, eth_location *loc)
{
  if (loc)
  {
    eth_t tracer = create_tracer(loc);
    int tracervid = new_val(bldr, RC_RULES_DISABLE);
    eth_write_insn(tape, eth_insn_cval(tracervid, tracer));

    int newexn = new_val(bldr, RC_RULES_DEFAULT);
    eth_insn *dotrace = eth_insn_apply(newexn, tracervid, &exnvid, 1);
    bldr->vinfo[newexn]->creatloc = dotrace;
    eth_write_insn(tape, dotrace);

    exnvid = newexn;
  }

  if (bldr->istry)
  {
    eth_insn *catch = eth_insn_catch(bldr->tryid, exnvid);
    eth_write_insn(tape, catch);
    trace_mov(bldr, catch);
  }
  else
    eth_write_insn(tape, eth_insn_ret(exnvid));
}

static void
assert_number(ssa_builder *bldr, eth_ssa_tape *tape, int vid, eth_location *loc,
    bool *e)
{
  if (bldr->vinfo[vid]->type)
  {
    if (bldr->vinfo[vid]->type != eth_number_type)
    {
      eth_warning("expression will fail: expected number, got %s",
          bldr->vinfo[vid]->type->name);
      *e = 1;
      int exn = new_val(bldr, RC_RULES_DISABLE);
      eth_write_insn(tape, eth_insn_cval(exn, eth_exn(eth_sym("Type_error"))));
      write_throw(bldr, tape, exn, loc);
    }
  }
  else
  {
    // test type:
    eth_ssa_tape *errtape = eth_create_ssa_tape();
    int exn = new_val(bldr, RC_RULES_DISABLE);
    eth_write_insn(errtape, eth_insn_cval(exn, eth_exn(eth_sym("Type_error"))));
    write_throw(bldr, errtape, exn, loc);
    // --
    eth_insn *test = eth_insn_if_test_type(-1, vid, eth_number_type,
        eth_insn_nop(), errtape->head);
    test->iff.likely = 1;
    eth_destroy_ssa_tape(errtape);
    // --
    eth_write_insn(tape, test);
  }
}

static eth_ssa_pattern*
build_pattern(ssa_builder *bldr, eth_ir_pattern *pat, int expr, bool myrules,
    bool *e)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
      return eth_ssa_dummy_pattern();

    case ETH_PATTERN_IDENT:
    {
      bldr->vars[pat->ident.varid] = expr;

      if (myrules)
        bldr->vinfo[expr]->rules = RC_RULES_DEFAULT;

      if (bldr->recscp)
        bldr->vinfo[expr]->isrec = true;

      return eth_ssa_ident_pattern(expr);
    }

    case ETH_PATTERN_UNPACK:
    {
      bool dotest;
      if (bldr->vinfo[expr]->type)
      {
        if (bldr->vinfo[expr]->type != pat->unpack.type)
        {
          eth_warning("pattern won't match: expect %s, but value is of type %s",
              pat->unpack.type->name, bldr->vinfo[expr]->type->name);
          *e = true;
          dotest = true;
        }
        else
          dotest = false;
      }
      else
        dotest = true;

      // fix type for inner branch
      set_type(bldr, expr, pat->unpack.type);

      // set up aliasing variable
      bldr->vars[pat->unpack.varid] = expr;

      eth_ssa_pattern *pats[pat->unpack.n];
      int vids[pat->unpack.n];
      for (int i = 0; i < pat->unpack.n; ++i)
      {
        vids[i] = new_val(bldr, RC_RULES_DISABLE);
        pats[i] = build_pattern(bldr, pat->unpack.subpats[i], vids[i], true, e);
      }

      return eth_ssa_unpack_pattern(pat->unpack.type, pat->unpack.offs, vids,
          pats, pat->unpack.n, dotest);
    }

    case ETH_PATTERN_CONSTANT:
    {
      value_info *vinfo = bldr->vinfo[expr];

      if (vinfo->cval and vinfo->cval == pat->constant.val)
        return eth_ssa_constant_pattern(pat->constant.val, false);

      if (vinfo->type and vinfo->type != pat->constant.val->type)
      {
        eth_warning("pattern won't match: expect %s, but value is of type %s",
            pat->constant.val->type->name, vinfo->type->name);
        *e = 1;
      }

      // fix type and value for inner branch
      set_cval(bldr, expr, pat->constant.val);
      set_type(bldr, expr, pat->constant.val->type);

      return eth_ssa_constant_pattern(pat->constant.val, true);
    }

    case ETH_PATTERN_RECORD:
    {
      // set up aliasing variable
      bldr->vars[pat->record.varid] = expr;

      eth_ssa_pattern *pats[pat->record.n];
      int vids[pat->record.n];
      for (int i = 0; i < pat->record.n; ++i)
      {
        vids[i] = new_val(bldr, RC_RULES_DISABLE);
        pats[i] = build_pattern(bldr, pat->record.subpats[i], vids[i], true, e);
      }
      return eth_ssa_record_pattern(pat->record.ids, vids, pats, pat->record.n);
    }
  }

  eth_error("wtf");
  abort();
}

/*
 * Translate tree-IR into SSA-instructions.
 */
static int
build(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *ir, int istc, bool *e)
{
  switch (ir->tag)
  {
    case ETH_IR_ERROR:
    {
      eth_error("ERROR-node passed to ssa-builder");
      *e = 1;
      return 0;
    }

    case ETH_IR_CVAL:
    {
      int ret = new_val(bldr, RC_RULES_DISABLE);
      bldr->vinfo[ret]->cval = ir->cval.val;
      bldr->vinfo[ret]->type = ir->cval.val->type;
      eth_write_insn(tape, eth_insn_cval(ret, ir->cval.val));
      return ret;
    }

    case ETH_IR_VAR:
    {
      int vid = bldr->vars[ir->var.vid];
      if (bldr->vinfo[vid]->isdummy)
      {
        eth_error("evaluating variable before initialization");
        *e = 1;
        return 0;
      }
      return vid;
    }

    case ETH_IR_APPLY:
    {
      // evaluate fn and args:
      int fn = build(bldr, tape, ir->apply.fn, false, e);
      int args[ir->apply.nargs];
      for (int i = 0; i < ir->apply.nargs; ++i)
        args[i] = build(bldr, tape, ir->apply.args[i], false, e);

      // apply:
      int ret = new_val(bldr, RC_RULES_DEFAULT);
      eth_insn *insn;
      if (istc && bldr->vinfo[fn]->rules == RC_RULES_DISABLE)
        insn = eth_insn_applytc(ret, fn, args, ir->apply.nargs);
      else
        insn = eth_insn_apply(ret, fn, args, ir->apply.nargs);
      bldr->vinfo[ret]->creatloc = insn;
      eth_write_insn(tape, insn);

      // handle exception:
      if (bldr->testexn)
      {
        eth_ssa_tape *thentape = eth_create_ssa_tape();
        write_throw(bldr, thentape, ret, ir->loc);

        eth_insn *test = eth_insn_if_test_type(-1, ret, eth_exception_type,
            thentape->head, eth_insn_nop());
        test->iff.likely = -1;
        eth_write_insn(tape, test);

        eth_destroy_ssa_tape(thentape);
      }

      return ret;
    }

    // TODO: collect similar code (see MATCH)
    case ETH_IR_IF:
    {
      int cond = build(bldr, tape, ir->iff.cond, false, e);

      eth_ssa_tape *thentape = eth_create_ssa_tape();
      int thenret = build_logical_block(bldr, thentape, ir->iff.thenbr, istc, e);

      eth_ssa_tape *elsetape = eth_create_ssa_tape();
      int elseret = build_logical_block(bldr, elsetape, ir->iff.elsebr, istc, e);

      int ret;
      eth_insn *loc = NULL;
      if (istc)
      {
        eth_write_insn(thentape, eth_insn_ret(thenret));
        eth_write_insn(elsetape, eth_insn_ret(elseret));
        ret = -1;
      }
      else
        ret = resolve_phi(bldr, thentape, thenret, elsetape, elseret);

      eth_insn *insn = eth_insn_if(ret, cond, thentape->head, elsetape->head);
      insn->iff.toplvl = ir->iff.toplvl;
      eth_destroy_ssa_tape(thentape);
      eth_destroy_ssa_tape(elsetape);

      eth_write_insn(tape, insn);
      if (ret >= 0)
        bldr->vinfo[ret]->creatloc = insn;
      return ret;
    }

    case ETH_IR_TRY:
    {
      eth_ssa_tape *trytape = eth_create_ssa_tape();
      // ---
      bldr->istry++;
      int oldid = bldr->tryid;
      int tryid = bldr->tryid = new_try(bldr);
      int tryret = build_logical_block(bldr, trytape, ir->try.trybr, false, e);
      bldr->istry--;
      bldr->tryid = oldid;

      eth_ssa_tape *cchtape = eth_create_ssa_tape();
      // ---
      int exnvid = new_val(bldr, RC_RULES_UNREF);
      eth_insn *getexn = eth_insn_getexn(exnvid);
      bldr->vinfo[exnvid]->creatloc = getexn;
      bldr->vars[ir->try.exnvar] = exnvid;
      eth_write_insn(cchtape, getexn);
      // ---
      int cchret = build_logical_block(bldr, cchtape, ir->try.catchbr, istc, e);

      int ret;
      eth_insn *loc = NULL;
      if (istc)
      {
        eth_write_insn(trytape, eth_insn_ret(tryret));
        eth_write_insn(cchtape, eth_insn_ret(cchret));
        ret = -1;
      }
      else
        ret = resolve_phi(bldr, trytape, tryret, cchtape, cchret);

      eth_insn *insn = eth_insn_try(ret, tryid, trytape->head, cchtape->head);
      insn->try.likely = ir->try.likely;
      eth_destroy_ssa_tape(trytape);
      eth_destroy_ssa_tape(cchtape);

      eth_write_insn(tape, insn);
      if (ret >= 0)
        bldr->vinfo[ret]->creatloc = insn;
      return ret;
    }

    case ETH_IR_SEQ:
    {
      int v1 = build(bldr, tape, ir->seq.e1, false, e);
      int v2 = build(bldr, tape, ir->seq.e2, istc, e);
      return v2;
    }

    case ETH_IR_STARTFIX:
    {
      start_rec_scope(bldr);

      int dummy = new_val(bldr, RC_RULES_DISABLE);
      bldr->vinfo[dummy]->isrec = true;
      bldr->vinfo[dummy]->isdummy = true;
      for (int i = 0; i < ir->startfix.n; ++i)
        bldr->vars[ir->startfix.vars[i]] = dummy;

      return build(bldr, tape, ir->startfix.body, istc, e);
    }

    case ETH_IR_ENDFIX:
    {
      for (int i = 0; i < ir->endfix.n; ++i)
        add_rec_variable(bldr, bldr->vars[ir->endfix.vars[i]]);
      finalize_rec_scope(bldr, tape, e);

      return build(bldr, tape, ir->endfix.body, istc, e);
    }

    case ETH_IR_BINOP:
    {
      int lhs = build(bldr, tape, ir->binop.lhs, false, e);
      int rhs = build(bldr, tape, ir->binop.rhs, false, e);

      bool testnum = false;
      int ret = -1;
      eth_insn *insn = NULL;
      switch (ir->binop.op)
      {
        case ETH_ADD ... ETH_POW:
        case ETH_LAND ... ETH_ASHR:
          ret = new_val(bldr, RC_RULES_DEFAULT);
          insn = eth_insn_binop(ir->binop.op, ret, lhs, rhs);

          testnum = true;
          bldr->vinfo[ret]->type = eth_number_type;
          bldr->vinfo[ret]->creatloc = insn;
          break;

        case ETH_LT ... ETH_NE:
          ret = new_val(bldr, RC_RULES_DISABLE);
          insn = eth_insn_binop(ir->binop.op, ret, lhs, rhs);

          testnum = true;
          bldr->vinfo[ret]->type = eth_boolean_type;
          bldr->vinfo[ret]->creatloc = insn;
          break;

        case ETH_IS:
        case ETH_EQUAL:
          ret = new_val(bldr, RC_RULES_DISABLE);
          insn = eth_insn_binop(ir->binop.op, ret, lhs, rhs);

          testnum = false;
          bldr->vinfo[ret]->type = eth_boolean_type;
          bldr->vinfo[ret]->creatloc = insn;
          break;

        case ETH_CONS:
          ret = new_val(bldr, RC_RULES_DEFAULT);
          insn = eth_insn_binop(ETH_CONS, ret, lhs, rhs);

          testnum = false;
          bldr->vinfo[ret]->type = eth_pair_type;
          bldr->vinfo[ret]->creatloc = insn;
          break;
      }

      if (testnum)
      {
        assert_number(bldr, tape, lhs, ir->loc, e);
        assert_number(bldr, tape, rhs, ir->loc, e);
        set_type(bldr, lhs, eth_number_type);
        set_type(bldr, rhs, eth_number_type);
      }

      eth_write_insn(tape, insn);
      return ret;
    }

    case ETH_IR_UNOP:
    {
      int expr = build(bldr, tape, ir->unop.expr, false, e);

      bool testnum = false;
      int ret = -1;
      eth_insn *insn = NULL;
      switch (ir->unop.op)
      {
        case ETH_NOT:
          ret = new_val(bldr, RC_RULES_DISABLE);
          insn = eth_insn_unop(ir->unop.op, ret, expr);
          testnum = false;
          bldr->vinfo[ret]->type = eth_boolean_type;
          break;

        case ETH_LNOT:
          ret = new_val(bldr, RC_RULES_DEFAULT);
          insn = eth_insn_unop(ir->unop.op, ret, expr);
          testnum = true;
          bldr->vinfo[ret]->type = eth_number_type;
          bldr->vinfo[ret]->creatloc = insn;
          break;
      }

      if (testnum)
        assert_number(bldr, tape, expr, ir->loc, e);

      eth_write_insn(tape, insn);
      return ret;
    }

    case ETH_IR_FN:
    {
      int arity = ir->fn.arity;

      if (bldr->recscp)
      {
        // create temporary dummy place-holder
        int ret = new_val(bldr, RC_RULES_DEFAULT);
        bldr->vinfo[ret]->type = eth_function_type;
        eth_insn *insn = eth_insn_alcfn(ret, arity);
        eth_write_insn(tape, insn);
        bldr->vinfo[ret]->creatloc = insn;
        // finalize closure at the end of the rec-scope
        deffer_closure(bldr, ret, ir);
        return ret;
      }
      else
        return build_fn(bldr, tape, ir, FN_NEW, e);
    }

    // TODO: collect similar code (see IF)
    case ETH_IR_MATCH:
    {
      int cond = build(bldr, tape, ir->match.expr, false, e);

      if (ir->match.pat->tag == ETH_PATTERN_IDENT)
        // optimize trivial identifier-match
      {
        eth_ssa_pattern *pat = build_pattern(bldr, ir->match.pat, cond, false, e);
        eth_destroy_ssa_pattern(pat);
        return build(bldr, tape, ir->match.thenbr, istc, e);
      }
      else
      {
        int lbstart = begin_logical_block(bldr);
        // --
        int n1 = bldr->vilen;
        eth_ssa_pattern *pat = build_pattern(bldr, ir->match.pat, cond, false, e);
        int n2 = bldr->vilen;
        // --
        eth_ssa_tape *thentape = eth_create_ssa_tape();
        eth_insn *ctor = eth_insn_nop();
        ctor->flag = ETH_IFLAG_NOBEFORE;
        eth_write_insn(thentape, ctor);
        int thenret = build(bldr, thentape, ir->match.thenbr, istc, e);
        // --
        end_logical_block(bldr, lbstart);

        eth_ssa_tape *elsetape = eth_create_ssa_tape();
        int elseret = build_logical_block(bldr, elsetape, ir->match.elsebr, istc, e);

        int ret;
        eth_insn *loc = NULL;
        if (istc)
        {
          eth_write_insn(thentape, eth_insn_ret(thenret));
          eth_write_insn(elsetape, eth_insn_ret(elseret));
          ret = -1;
        }
        else
          ret = resolve_phi(bldr, thentape, thenret, elsetape, elseret);

        eth_insn *insn = eth_insn_if_match(ret, cond, pat, thentape->head,
            elsetape->head);
        insn->iff.toplvl = ir->match.toplvl;
        eth_destroy_ssa_tape(thentape);
        eth_destroy_ssa_tape(elsetape);

        for (int i = n1; i < n2; ++i)
        {
          if (not bldr->vinfo[i]->creatloc)
            bldr->vinfo[i]->creatloc = ctor;
        }

        eth_write_insn(tape, insn);
        if (ret >= 0)
          bldr->vinfo[ret]->creatloc = insn;

        return ret;
      }

      case ETH_IR_MKRCRD:
      {
        int n = ir->mkrcrd.type->nfields;
        int vids[n];
        for (int i = 0; i < n; ++i)
          vids[i] = build(bldr, tape, ir->mkrcrd.fields[i], false, e);
        int out = new_val(bldr, RC_RULES_DEFAULT);
        eth_insn *insn = eth_insn_mkrcrd(out, vids, ir->mkrcrd.type);
        bldr->vinfo[out]->creatloc = insn;
        bldr->vinfo[out]->type = ir->mkrcrd.type;
        eth_write_insn(tape, insn);
        trace_mov(bldr, insn);
        return out;
      }
    }

    case ETH_IR_THROW:
    {
      int vid = build(bldr, tape, ir->throw.exn, false, e);
      write_throw(bldr, tape, vid, ir->loc);
      return vid;
    }
  }

  eth_error("undefined IR-node");
  *e = 1;
  return 0;
}

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             RC INSERTION
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/*
 * Check whether the sequence of instructins terminates with RET-instruction.
 */
static bool
is_dead_end(eth_insn *begin)
{
  for (eth_insn *insn = begin; insn; insn = insn->next)
  {
    if (insn->tag == ETH_INSN_IF)
    {
      int a = is_dead_end(insn->iff.thenbr);
      int b = is_dead_end(insn->iff.elsebr);
      return (a && b) || is_dead_end(insn->next);
    }

    // TODO: create marker-function
    if (insn->tag == ETH_INSN_RET or insn->tag == ETH_INSN_CATCH)
      return true;
  }

  return false;
}

/*
 * Check if instruction is referencing the ssa-value.
 */
static bool
is_using(eth_insn *insn, int vid)
{
  switch (insn->tag)
  {
    case ETH_INSN_NOP:
      return false;

    case ETH_INSN_CVAL:
      return false;

    case ETH_INSN_APPLY:
    case ETH_INSN_APPLYTC:
      if (insn->apply.fn == vid)
        return true;
      for (int i = 0; i < insn->apply.nargs; ++i)
      {
        if (insn->apply.args[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_IF:
      return insn->iff.cond == vid;

    case ETH_INSN_TRY:
      return false;

    case ETH_INSN_MOV:
      return insn->out == vid || insn->var.vid == vid;

    case ETH_INSN_REF ... ETH_INSN_DROP:
      return insn->var.vid == vid;

    case ETH_INSN_RET:
      return insn->var.vid == vid;

    case ETH_INSN_BINOP:
      return insn->binop.lhs == vid || insn->binop.rhs == vid;

    case ETH_INSN_UNOP:
      return insn->unop.vid == vid;

    case ETH_INSN_FN:
      for (int i = 0; i < insn->fn.ncap; ++i)
      {
        if (insn->fn.caps[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_FINFN:
      for (int i = 0; i < insn->finfn.ncap; ++i)
      {
        if (insn->finfn.caps[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_ALCFN:
      return false;

    case ETH_INSN_POP:
      return false;

    case ETH_INSN_CAP:
      return false;

    case ETH_INSN_MKSCP:
      for (int i = 0; i < insn->mkscp.nclos; ++i)
      {
        if (insn->mkscp.clos[i] == vid)
          return true;
      }
      for (int i = 0; i < insn->mkscp.nwref; ++i)
      {
        if (insn->mkscp.wrefs[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_CATCH:
      return insn->catch.vid == vid;

    case ETH_INSN_GETEXN:
      return false;

    case ETH_INSN_MKRCRD:
      for (int i = 0; i < insn->mkrcrd.type->nfields; ++i)
      {
        if (insn->mkrcrd.vids[i] == vid)
          return true;
      }
      return false;
  }

  eth_error("wtf");
  abort();
}

/*
 * Check if instruction is (potentially) killing the ssa-value.
 */
static bool
is_killing(eth_insn *insn, int vid)
{
  switch (insn->tag)
  {
    case ETH_INSN_APPLY:
    case ETH_INSN_APPLYTC:
      for (int i = 0; i < insn->apply.nargs; ++i)
      {
        if (insn->apply.args[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_RET:
      return insn->var.vid == vid;

    default:
      return false;
  }
}

static bool
is_moving(eth_insn *insn, int vid)
{
  switch (insn->tag)
  {
    case ETH_INSN_MOV:
      return insn->var.vid == vid;

    case ETH_INSN_FN:
      for (int i = 0; i < insn->fn.ncap; ++i)
      {
        if (insn->fn.caps[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_FINFN:
      for (int i = 0; i < insn->finfn.nmov; ++i)
      {
        if (insn->finfn.movs[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_CATCH:
      return insn->catch.vid == vid;

    case ETH_INSN_MKRCRD:
      for (int i = 0; i < insn->mkrcrd.type->nfields; ++i)
      {
        if (insn->mkrcrd.vids[i] == vid)
          return true;
      }
      return false;

    default:
      return false;
  }
}

static const int*
get_moved_vids(eth_insn *insn, int *n)
{
  switch (insn->tag)
  {
    case ETH_INSN_MOV:
      *n = 1;
      return &insn->var.vid;

    case ETH_INSN_FN:
      *n = insn->fn.ncap;
      return insn->fn.caps;

    case ETH_INSN_FINFN:
      *n = insn->finfn.nmov;
      return insn->finfn.movs;

    case ETH_INSN_CATCH:
      *n = 1;
      return &insn->catch.vid;

    case ETH_INSN_MKRCRD:
      *n = insn->mkrcrd.type->nfields;
      return insn->mkrcrd.vids;

    default:
      eth_error("wtf");
      abort();
  }
}

typedef struct {
  int nvals;
  struct {
    cod_vec(eth_insn*) killers;
  } *vals;
} kill_info;

static kill_info*
create_kill_info(int nvals)
{
  kill_info *kinfo = malloc(sizeof(kill_info));
  kinfo->nvals = nvals;
  kinfo->vals = malloc(sizeof(kinfo->vals[0]) * nvals);
  for (int i = 0; i < nvals; ++i)
    cod_vec_init(kinfo->vals[i].killers);
  return kinfo;
}

static void
destroy_kill_info(kill_info *kinfo)
{
  for (int i = 0; i < kinfo->nvals; ++i)
    cod_vec_destroy(kinfo->vals[i].killers);
  free(kinfo->vals);
  free(kinfo);
}

static void
add_killer(kill_info *kinfo, int vid, eth_insn *insn)
{
  cod_vec_push(kinfo->vals[vid].killers, insn);
}

static eth_insn*
last_insn(eth_insn *begin)
{
  while (begin->next)
    begin = begin->next;
  return begin;
}

static bool kill_value_T(kill_info *kinfo, eth_insn *begin, int vid);
static void kill_value_F(kill_info *kinfo, eth_insn *begin, int vid);

static void
force_kill(kill_info *kinfo, eth_insn *br, int vid)
{
  eth_insn *unref = eth_insn_unref(vid);
  eth_insert_insn_before(br, unref);
  add_killer(kinfo, vid, unref);
}

static void
kill_value_F(kill_info *kinfo, eth_insn *begin, int vid)
{
  for (eth_insn *insn = begin; insn; insn = insn->next)
  {
    if (insn->tag == ETH_INSN_IF)
    {
      eth_insn *b1 = insn->iff.thenbr;
      eth_insn *b2 = insn->iff.elsebr;

      if (is_dead_end(b1) and not kill_value_T(kinfo, b1, vid))
        force_kill(kinfo, b1, vid);
      if (is_dead_end(b2) and not kill_value_T(kinfo, b2, vid))
        force_kill(kinfo, b2, vid);
    }

    if (insn->tag == ETH_INSN_TRY)
    {
      eth_insn *t = insn->try.trybr;
      eth_insn *c = insn->try.catchbr;

      if (is_dead_end(c) and not kill_value_T(kinfo, c, vid))
        force_kill(kinfo, c, vid);
    }

    // TODO: create marker-function
    if (insn->tag == ETH_INSN_RET or insn->tag == ETH_INSN_CATCH)
    {
      eth_error("WTF!?");
      abort();
    }
  }
}

static bool
kill_value_T_if(kill_info *kinfo, eth_insn *insn, int vid)
{
  eth_insn *b1 = insn->iff.thenbr;
  eth_insn *b2 = insn->iff.elsebr;
  bool d1 = is_dead_end(b1);
  bool d2 = is_dead_end(b2);

  if ((d1 and d2) or not kill_value_T(kinfo, insn->next, vid))
  {
    bool k1 = kill_value_T(kinfo, b1, vid);
    bool k2 = kill_value_T(kinfo, b2, vid);
    if (k1 or k2)
    {
      if (not k1) force_kill(kinfo, b1, vid);
      if (not k2) force_kill(kinfo, b2, vid);
      return true;
    }
    else
      return false;
  }
  else
  {
    if (d1)
    {
      if (not kill_value_T(kinfo, b1, vid))
        force_kill(kinfo, b1, vid);
      kill_value_F(kinfo, b2, vid);
    }
    if (d2)
    {
      if (not kill_value_T(kinfo, b2, vid))
        force_kill(kinfo, b2, vid);
      kill_value_F(kinfo, b1, vid);
    }
    return true;
  }
}

static bool
kill_value_T_try(kill_info *kinfo, eth_insn *insn, int vid)
{
  eth_insn *t = insn->try.trybr;
  eth_insn *c = insn->try.catchbr;

  if (kill_value_T(kinfo, insn->next, vid))
  {
    if (is_dead_end(c))
    {
      if (not kill_value_T(kinfo, c, vid))
        force_kill(kinfo, c, vid);
    }
    else
      kill_value_F(kinfo, c, vid);
    return true;
  }
  else if (kill_value_T(kinfo, c, vid))
  {
    eth_insn *unref = eth_insn_unref(vid);
    eth_insert_insn_after(last_insn(t), unref);
    add_killer(kinfo, vid, unref);
    return true;
  }
  else if (kill_value_T(kinfo, t, vid))
    return true;
  else
    return false;
}

static bool
kill_value_T(kill_info *kinfo, eth_insn *begin, int vid)
{
  eth_insn *lastusr = NULL;

  for (eth_insn *insn = begin; insn; insn = insn->next)
  {
    if (is_using(insn, vid))
      lastusr = insn;

    if (insn->tag == ETH_INSN_IF)
    {
      if (kill_value_T_if(kinfo, insn, vid))
        return true;
      else
        break;
    }

    if (insn->tag == ETH_INSN_TRY)
    {
      if (kill_value_T_try(kinfo, insn, vid))
        return true;
      else
        break;
    }

    // TODO: create marker-function
    if (insn->tag == ETH_INSN_RET or insn->tag == ETH_INSN_CATCH)
      break;
  }

  if (lastusr == 0)
    return false;

  if (is_killing(lastusr, vid))
  {
    eth_insert_insn_before(lastusr, eth_insn_dec(vid));
    add_killer(kinfo, vid, lastusr);
  }
  else if (is_moving(lastusr, vid))
  {
    // do nothing in case of MOV (but report as if killed the value)
    add_killer(kinfo, vid, lastusr);
  }
  else
  {
    if (lastusr->tag == ETH_INSN_IF)
    {
      eth_insn *unref1 = eth_insn_unref(vid);
      eth_insn *unref2 = eth_insn_unref(vid);
      eth_insert_insn_before(lastusr->iff.thenbr, unref1);
      eth_insert_insn_before(lastusr->iff.elsebr, unref2);
      add_killer(kinfo, vid, unref1);
      add_killer(kinfo, vid, unref2);
    }
    else
    {
      eth_insn *unref = eth_insn_unref(vid);
      eth_insert_insn_after(lastusr, unref);
      add_killer(kinfo, vid, unref);
    }
  }
  return true;
}

static void
insert_rc_default(kill_info *kinfo, eth_insn *begin, int vid)
{
  if (kill_value_T(kinfo, begin, vid))
    eth_insert_insn_after(begin, eth_insn_ref(vid));
  else
    eth_insert_insn_after(begin, eth_insn_drop(vid));
}

static eth_insn*
find_mov(eth_insn *begin, int vid)
{
  if (begin == NULL) return NULL;
  if (begin->tag == ETH_INSN_MOV && begin->out == vid) return begin;
  return find_mov(begin->next, vid);
}

static void
insert_rc_phi(kill_info *kinfo, eth_insn *begin, int vid)
{
  eth_insn *br1, *br2;
  if (begin->tag == ETH_INSN_IF)
  {
    br1 = begin->iff.thenbr;
    br2 = begin->iff.elsebr;
  }
  else if (begin->tag == ETH_INSN_TRY)
  {
    br1 = begin->try.trybr;
    br2 = begin->try.catchbr;
  }
  else
    abort();

  if (not kill_value_T(kinfo, begin->next, vid))
  {
    // TODO: should remove that MOV at all
    eth_insn *mov;
    mov = find_mov(br1, vid);
    if (not kill_value_T(kinfo, mov->next, vid))
    {
      eth_insn *unref = eth_insn_unref(vid);
      eth_insert_insn_after(mov, unref);
      add_killer(kinfo, vid, unref);
    }
    mov = find_mov(br2, vid);
    if (not kill_value_T(kinfo, mov->next, vid))
    {
      eth_insn *unref = eth_insn_unref(vid);
      eth_insert_insn_after(mov, unref);
      add_killer(kinfo, vid, unref);
    }
  }
}

static void
insert_rc_unref(kill_info *kinfo, eth_insn *begin, int vid)
{
  if (not kill_value_T(kinfo, begin->next, vid))
  {
      eth_insn *unref = eth_insn_unref(vid);
      eth_insert_insn_after(begin, unref);
      add_killer(kinfo, vid, unref);
  }
}

static void
handle_movs(ssa_builder *bldr, kill_info *kinfo)
{
  for (size_t imov = 0; imov < bldr->movs.len; ++imov)
  {
    eth_insn *mov = bldr->movs.data[imov];
    int n;
    const int *vids = get_moved_vids(mov, &n);
    for (int i = 0; i < n; ++i)
    {
      int vid = vids[i];
      bool doref;

      if (bldr->vinfo[vid]->rules == RC_RULES_DISABLE)
      {
        doref = true;
      }
      else
      {
        doref = true;
        for (size_t ikill = 0; ikill < kinfo->vals[vid].killers.len; ++ikill)
        {
          if (kinfo->vals[vid].killers.data[ikill] == mov)
          {
            doref = false;
            break;
          }
        }
      }

      if (doref)
        eth_insert_insn_before(mov, eth_insn_ref(vid));
    }
  }
}

static void
insert_rc(ssa_builder *bldr, eth_insn *body)
{
  kill_info *kinfo = create_kill_info(bldr->vilen);

  for (int vid = 0; vid < bldr->vilen; ++vid)
  {
    value_info *vinfo = bldr->vinfo[vid];
    switch (vinfo->rules)
    {
      case RC_RULES_DEFAULT:
        assert(vinfo->creatloc);
        insert_rc_default(kinfo, vinfo->creatloc, vid);
        break;

      case RC_RULES_PHI:
        assert(vinfo->creatloc);
        insert_rc_phi(kinfo, vinfo->creatloc, vid);
        break;

      case RC_RULES_UNREF:
        assert(vinfo->creatloc);
        insert_rc_unref(kinfo, vinfo->creatloc, vid);
        break;

      case RC_RULES_DISABLE:
        break;
    }
  }
  handle_movs(bldr, kinfo);
  destroy_kill_info(kinfo);
}


void
eth_destroy_ssa(eth_ssa *ssa)
{
  eth_destroy_insn_list(ssa->body);
  free(ssa);
}

static eth_ssa*
build_body(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *body, bool *e)
{
  int fnret = build_logical_block(bldr, tape, body, true, e);
  eth_write_insn(tape, eth_insn_ret(fnret));
  insert_rc(bldr, tape->head);
  return create_ssa(tape->head, bldr->vilen, bldr->ntries);
}

typedef struct {
  int ndefs;
  char **idents;
} module_info;

static void
destroy_module_info(void *ptr)
{
  module_info *data = ptr;
  for (int i = 0; i < data->ndefs; ++i)
    free(data->idents[i]);
  free(data->idents);
  free(data);
}

static eth_t
make_module(void)
{
  module_info *data = eth_this->proc.data;
  eth_t acc = eth_nil;
  for (int i = data->ndefs - 1; i >= 0; --i)
  {
    eth_t keyval = eth_cons(eth_str(data->idents[i]), eth_sp[i]);
    acc = eth_cons(keyval, acc);
  }
  eth_pop_stack(data->ndefs);
  return acc;
}

static eth_insn*
find_last_toplevel_insn(eth_insn *insn)
{
  if (insn->tag == ETH_INSN_IF)
  {
    switch (insn->iff.toplvl)
    {
      case ETH_TOPLVL_THEN:
        return find_last_toplevel_insn(insn->iff.thenbr);

      case ETH_TOPLVL_ELSE:
        return find_last_toplevel_insn(insn->iff.elsebr);

      case ETH_TOPLVL_NONE:
        break;
    }
  }

  if (insn->next == NULL)
    return insn;
  else
    return find_last_toplevel_insn(insn->next);
}

eth_ssa*
eth_build_ssa(eth_ir *ir, eth_ir_defs *defs)
{
  eth_ssa *ssa = NULL;
  bool e = false;

  ssa_builder *bldr = create_ssa_builder(ir->nvars);
  eth_ssa_tape *tape = eth_create_ssa_tape();
  int ret = build_logical_block(bldr, tape, ir->body, false, &e);
  if (e)
  {
    eth_destroy_insn_list(tape->head);
    ssa = NULL;
  }
  else
  {
    if (defs)
    {
      // TODO: into separate function
      eth_insn *lastinsn = find_last_toplevel_insn(tape->head);
      eth_ssa_tape *tlvltape;
      if (lastinsn == tape->point)
        tlvltape = tape;
      else
        tlvltape = eth_create_ssa_tape_at(lastinsn);

      int n = defs->n;
      module_info *data = malloc(sizeof(module_info));
      data->ndefs = n;
      data->idents = malloc(sizeof(char*) * n);
      for (int i = 0; i < n; ++i)
        data->idents[i] = strdup(defs->idents[i]);

      eth_t mkmod = eth_create_proc(make_module, n, data, destroy_module_info);

      int modret = new_val(bldr, RC_RULES_DEFAULT);
      int fnvid = new_val(bldr, RC_RULES_DISABLE);
      eth_write_insn(tlvltape, eth_insn_cval(fnvid, mkmod));
      int argvids[n];
      for (int i = 0; i < n; ++i)
        argvids[i] = bldr->vars[defs->varids[i]];

      eth_insn *insn = eth_insn_apply(modret, fnvid, argvids, n);
      eth_write_insn(tlvltape, insn);
      bldr->vinfo[modret]->creatloc = insn;

      eth_write_insn(tlvltape, eth_insn_ret(modret));
      eth_write_insn(tlvltape, eth_insn_ret(ret));

      if (tlvltape != tape)
        eth_destroy_ssa_tape(tlvltape);

      int exnvid = new_val(bldr, RC_RULES_DISABLE);
      eth_t exn = eth_exn(eth_sym("Import_error"));
      eth_write_insn(tape, eth_insn_cval(exnvid, exn));
      eth_write_insn(tape, eth_insn_ret(exnvid));
    }
    else
    {
      eth_write_insn(tape, eth_insn_ret(ret));
    }

    insert_rc(bldr, tape->head);
    ssa = create_ssa(tape->head, bldr->vilen, bldr->ntries);
  }
  eth_destroy_ssa_tape(tape);
  destroy_ssa_builder(bldr);

  return ssa;
}

