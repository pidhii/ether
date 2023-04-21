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
#include <string.h>
#include <assert.h>


ETH_MODULE("ether:ssa-builder")


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                               BUILDER
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef enum {
  /**
   * @brief Default RC rules for temporaries.
   *
   * **RC Policy**
   * Initialy an object is not referenced.
   * If \ref kill_value_T() handled the rease of an object, it will be
   * referenced right after it is created and kept referenced untill end if its
   * lifetime. Otherwize, object will be dropped right after the creation.
   */
  RC_RULES_DEFAULT,

  /**
   * @brief Disable automatic reference counting.
   */
  RC_RULES_DISABLE,

  /**
   * @brief Rules for Phi-nodes.
   *
   * ONLY allowed for true Phi-nodes, i.e. SSA temporaries owned by the IF- and
   * TRY-forms.
   *
   * **RC Policy**
   * Object MUST be referenced by hand at MOV instructions. Note, it does not
   * imply, in general, the need for REF instruction. The statement stress that
   * it just has to be *referenced* without specifying a way to ensure/acomplish
   * this. Object is fed to @ref kill_value_T() launched at the instruction next
   * to the parent form (IF/TRY). If *user* is found and thus release of the
   * object is handled by the algorithm, we are done. Otherwize, lifetime is
   * defined branch-wise. Each branch is handled separately and independently of
   * the others. For each branch:
   * 1. lifetime start is at MOV (it must always be present).
   * ..TODO..
   */
  RC_RULES_PHI,

  RC_RULES_UNREF,
} rc_rules;

typedef struct {
  int vid;
  size_t fid;
} field_info;

typedef struct {
  rc_rules rules;
  eth_t cval;
  eth_type *type;
  eth_insn *creatloc;
  cod_vec(field_info) fields;
  bool isthis;
} ssa_value_info;

static ssa_value_info*
create_empty_ssa_value_info()
{
  ssa_value_info *ssavinfo = calloc(1, sizeof(ssa_value_info));
  cod_vec_init(ssavinfo->fields);
  return ssavinfo;
}

static void
destroy_ssa_value_info(ssa_value_info *ssavinfo)
{
  cod_vec_destroy(ssavinfo->fields);
  free(ssavinfo);
}

static void
copy_ssa_value_info(ssa_value_info *dst, ssa_value_info *src)
{
  dst->cval = src->cval;
  dst->type = src->type;
}

typedef struct ir_variable_info {
  int ssavid;
  eth_type *type;
} ir_variable_info;

static ir_variable_info*
create_empty_ir_variable_info()
{
  ir_variable_info *irvinfo = calloc(1, sizeof(ir_variable_info));
  return irvinfo;
}

static void
destroy_ir_variable_info(ir_variable_info *irvinfo)
{
  free(irvinfo);
}


typedef enum {
  ACTION_SETCVAL,
  ACTION_SETTYPE,
  ACTION_SETFIELD,
} action_tag;

typedef struct {
  action_tag tag;
  union {
    struct { int vid; eth_t new, old; } setcval;
    struct { int vid; eth_type *new, *old; } settype;
    struct { int vid; size_t fid; } setfield;
  };
} action;

typedef struct {
  int nirvars;
  ir_variable_info **irvinfo;
  // TODO: use cod_vec
  int nssavals;
  int vicap;
  ssa_value_info **ssavinfo;
  cod_vec(eth_insn*) movs;
  bool testexn;
  int ntries;
  int istry, tryid;
  cod_vec(action) actions;
} ssa_builder;

static ssa_builder*
create_ssa_builder(int nirvars)
{
  ssa_builder *bldr = malloc(sizeof(ssa_builder));
  bldr->nirvars = nirvars;
  bldr->irvinfo = malloc(sizeof(ir_variable_info*) * nirvars);
  for (int i = 0; i < nirvars; ++i)
    bldr->irvinfo[i] = create_empty_ir_variable_info();
  bldr->nssavals = 0;
  bldr->vicap = 0x40;
  bldr->ssavinfo = malloc(sizeof(ssa_value_info*) * bldr->vicap);
  cod_vec_init(bldr->movs);
  bldr->testexn = true;
  bldr->ntries = 0;
  bldr->istry = 0;
  cod_vec_init(bldr->actions);
  return bldr;
}

static void
destroy_ssa_builder(ssa_builder *bldr)
{
  for (int i = 0; i < bldr->nirvars; ++i)
    destroy_ir_variable_info(bldr->irvinfo[i]);
  free(bldr->irvinfo);
  for (int i = 0; i < bldr->nssavals; ++i)
    destroy_ssa_value_info(bldr->ssavinfo[i]);
  free(bldr->ssavinfo);
  cod_vec_destroy(bldr->movs);
  assert(bldr->actions.len == 0);
  cod_vec_destroy(bldr->actions);
  free(bldr);
}

static int
begin_logical_block(ssa_builder *bldr)
{ return bldr->actions.len; }

static void
set_cval(ssa_builder *bldr, int vid, eth_t val)
{
  cod_vec_push(bldr->actions, (action) {
    .tag = ACTION_SETCVAL,
    .setcval = {
      .vid = vid,
      .new = val,
      .old = bldr->ssavinfo[vid]->cval,
    }
  });
  bldr->ssavinfo[vid]->cval = val;
  bldr->ssavinfo[vid]->type = val->type;
}

static void
set_type(ssa_builder *bldr, int vid, eth_type *type)
{
  cod_vec_push(bldr->actions, (action) {
    .tag = ACTION_SETTYPE,
    .settype = {
      .vid = vid,
      .new = type,
      .old = bldr->ssavinfo[vid]->type,
    }
  });
  bldr->ssavinfo[vid]->type = type;
}

// XXX: use this!!
static void
set_field(ssa_builder *bldr, int vid, size_t fid, int fldvid)
{
  cod_vec_push(bldr->actions, (action) {
    .tag = ACTION_SETFIELD,
    .setfield = {
      .vid = vid,
      .fid = fid,
    }
  });
  field_info info = { .fid = fid, .vid = fldvid };
  cod_vec_push(bldr->ssavinfo[vid]->fields, info);
}

static void
undo_action(ssa_builder *bldr, action *a)
{
  switch (a->tag)
  {
    case ACTION_SETCVAL:
      if ((bldr->ssavinfo[a->setcval.vid]->cval = a->setcval.old))
        bldr->ssavinfo[a->setcval.vid]->type = a->setcval.old->type;
      else
        bldr->ssavinfo[a->setcval.vid]->type = NULL;
      break;

    case ACTION_SETTYPE:
      bldr->ssavinfo[a->settype.vid]->type = a->settype.old;
      break;

    case ACTION_SETFIELD:
    {
      ssa_value_info *ssavinfo = bldr->ssavinfo[a->setfield.vid];
      for (size_t i = 0; i < ssavinfo->fields.len; ++i)
      {
        if (ssavinfo->fields.data[i].fid == a->setfield.fid)
        {
          cod_vec_erase(ssavinfo->fields, i);
          return;
        }
      }
      assert(!"ωτφ");
    }
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
  if (bldr->vicap == bldr->nssavals)
  {
    bldr->vicap <<= 1;
    bldr->ssavinfo = realloc(bldr->ssavinfo, sizeof(ssa_value_info*) * bldr->vicap);
  }
  int vid = bldr->nssavals++;
  bldr->ssavinfo[vid] = create_empty_ssa_value_info();
  bldr->ssavinfo[vid]->rules = rules;
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
  ssa->rc = 0;
  ssa->nvals = nvals;
  ssa->ntries = ntries;
  ssa->body = body;
  return ssa;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               build SSA
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define FN_NEW (-1)
static int
build_fn(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *fn, int selfscpidx,
    bool *e);

static int
build(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *ir, bool istc, bool *e);

static int
build_logical_block(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *ir,
    int istc, bool *e)
{
  int lbstart = begin_logical_block(bldr);
  int ret = build(bldr, tape, ir, istc, e);
  end_logical_block(bldr, lbstart);
  return ret;
}

static void
move_to_phi_default(ssa_builder *bldr, eth_ssa_tape *tape, int phi, int vid)
{
  switch (bldr->ssavinfo[vid]->rules)
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
  int rc1 = bldr->ssavinfo[r1]->rules;
  int rc2 = bldr->ssavinfo[r2]->rules;
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
build_fn(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *ir, int selfscpidx,
    bool *e)
{
  int arity = ir->fn.arity;

  // enclose outer variables:
  int ncap = ir->fn.ncap;
  int capvids_parent[ncap];
  for (int i = 0; i < ncap; ++i)
    capvids_parent[i] = bldr->irvinfo[ir->fn.caps[i]]->ssavid;

  eth_capvar_info capinfo[ncap];
  for (int i = 0; i < ncap; ++i)
  {
    capinfo[i].varid_local = ir->fn.capvars[i];
    capinfo[i].type = bldr->ssavinfo[capvids_parent[i]]->type;
    capinfo[i].cval = bldr->ssavinfo[capvids_parent[i]]->cval;
  }
  eth_ssa *ssa = eth_build_fn_body(ir->fn.body, arity, capinfo, ncap,
        ir->fn.scpvars_local, ir->fn.nscpvars, selfscpidx, e);

  int ret = new_val(bldr, RC_RULES_DEFAULT);
  eth_insn *insn =
    eth_insn_fn(ret, arity, capvids_parent, ncap, ir->fn.ast, ir->fn.body, ssa);
  bldr->ssavinfo[ret]->creatloc = insn;
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
write_throw(ssa_builder *bldr, eth_ssa_tape *tape, int exnvid,
    eth_location *loc)
{
  if (loc)
  {
    eth_t tracer = create_tracer(loc);
    int tracervid = new_val(bldr, RC_RULES_DISABLE);
    eth_write_insn(tape, eth_insn_cval(tracervid, tracer));

    int newexn = new_val(bldr, RC_RULES_DEFAULT);
    eth_insn *dotrace = eth_insn_apply(newexn, tracervid, &exnvid, 1);
    bldr->ssavinfo[newexn]->creatloc = dotrace;
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
  if (bldr->ssavinfo[vid]->type)
  {
    if (bldr->ssavinfo[vid]->type != eth_number_type)
    {
      eth_warning("expression will fail: expected number, got %s",
          bldr->ssavinfo[vid]->type->name);
      *e = 1;
      int exn = new_val(bldr, RC_RULES_DISABLE);
      eth_write_insn(tape, eth_insn_cval(exn, eth_exn(eth_type_error())));
      write_throw(bldr, tape, exn, loc);
    }
  }
  else
  {
    // test type:
    eth_ssa_tape *errtape = eth_create_ssa_tape();
    int exn = new_val(bldr, RC_RULES_DISABLE);
    eth_write_insn(errtape, eth_insn_cval(exn, eth_exn(eth_type_error())));
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

typedef enum match_result {
  MATCH_UNKNOWN,
  MATCH_SUCCESS,
  MATCH_FAILURE,
} match_result;

static match_result
combine_match_results(match_result a, match_result b)
{
  if (a == MATCH_FAILURE or b == MATCH_FAILURE)
    return MATCH_FAILURE;

  if (a == MATCH_UNKNOWN or b == MATCH_UNKNOWN)
    return MATCH_UNKNOWN;

  // at this point it is guaranteed that both
  // - a == MATCH_SUCCESS, and
  // - b == MATCH_SUCCESS
  return MATCH_SUCCESS;
}

/**
 * @brief Build SSA-pattern.
 *
 * @param[in,out] bldr IR builder.
 * @param pat IR-pattern.
 * @param expr SSA value identifier refering to the expression to be matched by
 *  the pattern.
 * @param[out] e Will be set to `true` in case of errors.
 *
 * @return SSA-pattern.
 */
static eth_ssa_pattern*
build_pattern(ssa_builder *bldr, const eth_ir_pattern *pat, int expr,
    match_result *mchres, bool *e)
{
  match_result dummy_mchres;
  if (mchres == NULL)
    mchres = &dummy_mchres;

  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
      *mchres = MATCH_SUCCESS;
      return eth_ssa_dummy_pattern();

    case ETH_PATTERN_IDENT:
    {
      bldr->irvinfo[pat->ident.varid]->ssavid = expr;
      *mchres = MATCH_SUCCESS;
      return eth_ssa_ident_pattern(expr);
    }

    case ETH_PATTERN_UNPACK:
    {
      match_result mymchres = MATCH_UNKNOWN;

      // TODO: handle cval
      if (bldr->ssavinfo[expr]->type)
      {
        if (bldr->ssavinfo[expr]->type != pat->unpack.type)
          mymchres = MATCH_FAILURE;
        else
          mymchres = MATCH_SUCCESS;
      }

      // fix type for inner branch
      set_type(bldr, expr, pat->unpack.type);

      // set up aliasing variable
      bldr->irvinfo[pat->unpack.varid]->ssavid = expr;

      eth_ssa_pattern *pats[pat->unpack.n];
      int vids[pat->unpack.n];
      for (int i = 0; i < pat->unpack.n; ++i)
      {
        vids[i] = new_val(bldr, RC_RULES_DEFAULT);
        match_result submchres;
        pats[i] = build_pattern(bldr, pat->unpack.subpats[i], vids[i],
            &submchres, e);
        mymchres = combine_match_results(mymchres, submchres);
      }

      *mchres = mymchres;
      return eth_ssa_unpack_pattern(pat->unpack.type, pat->unpack.offs, vids,
          pats, pat->unpack.n, mymchres != MATCH_SUCCESS);
    }

    case ETH_PATTERN_CONSTANT:
    {
      ssa_value_info *ssavinfo = bldr->ssavinfo[expr];

      // # NIL & BOOLEANs & SYMBOLs
      if (pat->constant.val->type == eth_nil_type
          or pat->constant.val->type == eth_boolean_type
          or pat->constant.val->type == eth_symbol_type)
      {
        if (ssavinfo->cval and ssavinfo->cval == pat->constant.val)
        {
          *mchres = MATCH_SUCCESS;
          return eth_ssa_constant_pattern(pat->constant.val, ETH_TEST_IS, false);
        }
        else if (ssavinfo->type and ssavinfo->type != pat->constant.val->type)
        {
          *mchres = MATCH_FAILURE;
          return eth_ssa_constant_pattern(pat->constant.val, ETH_TEST_IS, true);
        }
        else
        {
          // fix type and value for inner branch
          set_cval(bldr, expr, pat->constant.val);
          *mchres = MATCH_UNKNOWN;
          return eth_ssa_constant_pattern(pat->constant.val, ETH_TEST_IS, true);
        }
      }
      // # STRINGs and NUMBERs
      else if (pat->constant.val->type == eth_string_type
          or pat->constant.val->type == eth_number_type)
      {
        if (ssavinfo->cval and eth_equal(ssavinfo->cval, pat->constant.val))
        {
          *mchres = MATCH_SUCCESS;
          return eth_ssa_constant_pattern(pat->constant.val, ETH_TEST_EQUAL,
              false);
        }
        else if (ssavinfo->type and ssavinfo->type != pat->constant.val->type)
        {
          *mchres = MATCH_FAILURE;
          return eth_ssa_constant_pattern(pat->constant.val, ETH_TEST_EQUAL,
              true);
        }
        else
        {
          // fix type and value for inner branch
          set_cval(bldr, expr, pat->constant.val);
          *mchres = MATCH_UNKNOWN;
          return eth_ssa_constant_pattern(pat->constant.val, ETH_TEST_EQUAL,
              true);
        }
      }
      else
      {
        eth_error("unsupported constant for match");
        abort();
      }
    }

    case ETH_PATTERN_RECORD:
    {
      // set up aliasing variable
      bldr->irvinfo[pat->record.varid]->ssavid = expr;

      match_result mymchres = MATCH_UNKNOWN;
#if 1
      if (bldr->ssavinfo[expr]->type)
      {
        eth_type *type = bldr->ssavinfo[expr]->type;
        int nmchfields = pat->record.n;
        //int fieldids[nmchfields];
        int fieldoffs[nmchfields];
        for (int i = 0; i < nmchfields; ++i)
        {
          int fieldidx = eth_get_field_by_id(type, pat->record.ids[i]);
          if (fieldidx == type->nfields)
          { // wont match
            mymchres = MATCH_FAILURE;
            goto l_match_record_without_vinfo;
          }
          eth_field *field = &type->fields[fieldidx];
          fieldoffs[i] = field->offs;
        }

        // TODO:
#if 0
        if (bldr->ssavinfo[expr]->cval)
        {
          int vids[nmchfields];
          for (int i = 0; i < nmchfields; ++i)
          {
            vids[i] = new_val(bldr, RC_RULES_DISABLE);
          }
        }
        else
#endif
        { // convert into UNPACK
          eth_ssa_pattern *pats[nmchfields];
          int vids[nmchfields];
          for (int i = 0; i < nmchfields; ++i)
          {
            vids[i] = new_val(bldr, RC_RULES_DEFAULT);
            match_result submchres;
            pats[i] = build_pattern(bldr, pat->record.subpats[i], vids[i],
                &submchres, e);
            mymchres = combine_match_results(mymchres, submchres);
          }

          *mchres = mymchres;
          return eth_ssa_unpack_pattern(type, fieldoffs, vids, pats, nmchfields,
              true);
        }
      }
#endif

l_match_record_without_vinfo:
      eth_ssa_pattern *pats[pat->record.n];
      int vids[pat->record.n];
      for (int i = 0; i < pat->record.n; ++i)
      {
        vids[i] = new_val(bldr, RC_RULES_DEFAULT);
        match_result submchres;
        pats[i] = build_pattern(bldr, pat->record.subpats[i], vids[i],
            &submchres, e);
        mymchres = combine_match_results(mymchres, submchres);
      }
      *mchres = mymchres;
      return eth_ssa_record_pattern(pat->record.ids, vids, pats, pat->record.n);
    }
  }

  eth_error("wtf");
  abort();
}

static inline bool
is_wildcard(const eth_ir_pattern *pat)
{
  return pat->tag == ETH_PATTERN_DUMMY
      or pat->tag == ETH_PATTERN_IDENT
  ;
}

/*
 * Select column following `First Row'-heuristic.
 */
static inline int
select_column(const eth_ir_match_table *P)
{
  assert(P->h > 0);
  for (int i = 0; i < P->w; ++i)
  {
    if (not is_wildcard(P->tab[0][i]))
      return i;
  }
  abort();
}

static bool
cmp_patterns(const eth_ir_pattern *p1, const eth_ir_pattern *p2)
{
  if (is_wildcard(p1) or is_wildcard(p2))
  {
    return true;
  }
  else if (p1->tag != p2->tag)
  {
    return false;
  }
  else
  {
    switch (p1->tag)
    {
      case ETH_PATTERN_UNPACK:
      {
        if (p1->unpack.type != p2->unpack.type)
          return false;
        if (p1->unpack.n != p1->unpack.type->nfields or
            p2->unpack.n == p2->unpack.type->nfields)
        {
          eth_error("unsupported multimatch pattern");
          abort();
        }
        return true;
      }

      default:
        eth_error("unsupported multimatch pattern");
        abort();
    }
  }
}

static int
get_pattern_arity(const eth_ir_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
    case ETH_PATTERN_IDENT:
    case ETH_PATTERN_CONSTANT:
      return 1;

    case ETH_PATTERN_UNPACK:
      return pat->unpack.n;

    default:
      eth_error("unsupported multimatch pattern");
      abort();
  }
}

static eth_mtree*
build_mtree(ssa_builder *bldr, const eth_ir_match_table *P, int o[], int phi,
    bool istc, bool *e)
{
  /*
   * If matrix P has no row then matching always fails, since there is not row
   * to match.
   *
   *   CC(o, Ø → A) = Fail
   */
  if (P->h == 0)
    return eth_create_fail();

  /*
   * If the first row of P exists and is constituted by wildcards, then matching
   * always succeeds and yeilds the first action.
   *
   *         _   ... _   → a1
   *   CC(o,     ...          ) = Leaf(a1)
   *         pm1 ... pmn → am
   */
  bool allwild = true;
  for (int i = 0; i < P->w; ++i)
  {
    if (not is_wildcard(P->tab[0][i]))
    {
      allwild = false;
      break;
    }
  }
  if (allwild)
  {
    eth_ssa_tape *tape = eth_create_ssa_tape();
    // --
    eth_insn *entry = eth_insn_nop();
    entry->flag |= ETH_IFLAG_NOBEFORE;
    eth_write_insn(tape, entry);
    int ret = build(bldr, tape, P->exprs[0], istc, e);
    // --
    if (istc)
      eth_write_insn(tape, eth_insn_ret(ret));
    else
      move_to_phi_default(bldr, tape, phi, ret);
    // --
    eth_mtree *leaf = eth_create_leaf(tape->head);
    eth_destroy_ssa_tape(tape);
    return leaf;
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   * Build Switch.
   */
  /*
   * Pick a column.
   */
  int l = select_column(P);

  /*
   * Construct a set H of constructor equivalence classes.
   */
  typedef cod_vec(int) int_vec;
  cod_vec(int_vec) H; /* Set of equivalence classes. */
  cod_vec_init(H);
  bool visited[P->h]; /* Will mark visited rows. */
  memset(visited, 0, sizeof visited);
  for (int i = 0; i < P->h; ++i)
  {
    if (visited[i])
      continue;

    /* Pick a representative for a new equivalence class. */
    const eth_ir_pattern *c = P->tab[i][l];
    /* Now scann remaining rows and accumulate the equivalance class. */
    int_vec cclass;
    cod_vec_init(cclass);
    for (int j = i; j < P->h; ++j)
    {
      if (visited[i])
        continue;
      if (cmp_patterns(c, P->tab[j][l]))
      {
        cod_vec_push(cclass, j);
        visited[j] = true;
      }
    }
    cod_vec_push(H, cclass);
  }

  for (size_t ic = 0; ic < H.len; ++ic)
  {
    int arity = get_pattern_arity(P->tab[H.data[ic].data[0]][l]);
    int no = P->w - 1 + arity;
    int na = H.data[ic].len;

    /* Create new SSA-vids for unpacked fields. */

    /* Construct new object-vector. */
    int oc[no];
    for (int i = 0; i < no; ++i)
    {
      if (i < l)
        oc[i] = o[i];
      else if (i < l + arity)
        /*oc[i] = */
        {};
    }
  }
}

/*
 * Translate tree-IR into SSA-instructions.
 */
static int
build(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *ir, bool istc, bool *e)
{
  switch (ir->tag)
  {
    case ETH_IR_ERROR:
    {
      eth_error("ERROR-node passed to SSA-builder");
      *e = 1;
      return 0;
    }

    case ETH_IR_CVAL:
    {
      int ret = new_val(bldr, RC_RULES_DISABLE);
      bldr->ssavinfo[ret]->cval = ir->cval.val;
      bldr->ssavinfo[ret]->type = ir->cval.val->type;
      eth_write_insn(tape, eth_insn_cval(ret, ir->cval.val));
      return ret;
    }

    case ETH_IR_VAR:
    {
      int vid = bldr->irvinfo[ir->var.vid]->ssavid;
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
      int ret;
      eth_insn *insn;
      if (istc && bldr->ssavinfo[fn]->rules == RC_RULES_DISABLE)
      {
        if (bldr->ssavinfo[fn]->isthis)
        {
          eth_write_insn(tape, eth_insn_loop(args, ir->apply.nargs));
          return -1;
        }
        else
        {
          ret = new_val(bldr, RC_RULES_DEFAULT);
          insn = eth_insn_applytc(ret, fn, args, ir->apply.nargs);
        }
      }
      else
      {
        ret = new_val(bldr, RC_RULES_DEFAULT);
        insn = eth_insn_apply(ret, fn, args, ir->apply.nargs);
      }
      bldr->ssavinfo[ret]->creatloc = insn;
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
      insn->iff.likely = ir->iff.likely;
      eth_destroy_ssa_tape(thentape);
      eth_destroy_ssa_tape(elsetape);

      eth_write_insn(tape, insn);
      if (ret >= 0)
        bldr->ssavinfo[ret]->creatloc = insn;
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
      bldr->ssavinfo[exnvid]->type = eth_exception_type;
      eth_insn *getexn = eth_insn_getexn(exnvid);
      bldr->ssavinfo[exnvid]->creatloc = getexn;
      bldr->irvinfo[ir->try.exnvar]->ssavid = exnvid;
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
        bldr->ssavinfo[ret]->creatloc = insn;
      return ret;
    }

    case ETH_IR_SEQ:
    {
      int v1 = build(bldr, tape, ir->seq.e1, false, e);
      int v2 = build(bldr, tape, ir->seq.e2, istc, e);
      return v2;
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
          bldr->ssavinfo[ret]->type = eth_number_type;
          bldr->ssavinfo[ret]->creatloc = insn;
          break;

        case ETH_LT ... ETH_GE:
          ret = new_val(bldr, RC_RULES_DEFAULT);
          insn = eth_insn_binop(ir->binop.op, ret, lhs, rhs);

          testnum = true;
          bldr->ssavinfo[ret]->creatloc = insn;
          break;

        case ETH_EQ ... ETH_NE:
          ret = new_val(bldr, RC_RULES_DISABLE);
          insn = eth_insn_binop(ir->binop.op, ret, lhs, rhs);

          testnum = true;
          bldr->ssavinfo[ret]->type = eth_boolean_type;
          bldr->ssavinfo[ret]->creatloc = insn;
          break;

        case ETH_IS:
        case ETH_EQUAL:
          ret = new_val(bldr, RC_RULES_DISABLE);
          insn = eth_insn_binop(ir->binop.op, ret, lhs, rhs);

          testnum = false;
          bldr->ssavinfo[ret]->type = eth_boolean_type;
          bldr->ssavinfo[ret]->creatloc = insn;
          break;

        case ETH_CONS:
          ret = new_val(bldr, RC_RULES_DEFAULT);
          insn = eth_insn_binop(ETH_CONS, ret, lhs, rhs);
          trace_mov(bldr, insn);

          testnum = false;
          bldr->ssavinfo[ret]->type = eth_pair_type;
          bldr->ssavinfo[ret]->creatloc = insn;
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
          bldr->ssavinfo[ret]->type = eth_boolean_type;
          break;

        case ETH_LNOT:
          ret = new_val(bldr, RC_RULES_DEFAULT);
          insn = eth_insn_unop(ir->unop.op, ret, expr);
          testnum = true;
          bldr->ssavinfo[ret]->type = eth_number_type;
          bldr->ssavinfo[ret]->creatloc = insn;
          break;
      }

      if (testnum)
        assert_number(bldr, tape, expr, ir->loc, e);

      eth_write_insn(tape, insn);
      return ret;
    }

    case ETH_IR_FN:
      return build_fn(bldr, tape, ir, FN_NEW, e);

    case ETH_IR_LETREC:
    {
      int lbstart = begin_logical_block(bldr);

      // init scope vars
      int vids[ir->letrec.nvars];
      for (int i = 0; i < ir->letrec.nvars; ++i)
      {
        vids[i] = build_fn(bldr, tape, ir->letrec.exprs[i], i, e);
        int varid = ir->letrec.varids[i];
        bldr->irvinfo[varid]->ssavid = vids[i];
        set_type(bldr, vids[i], eth_function_type);
      }
      // add scope ctor
      eth_insn *mkscp = eth_insn_mkscp(vids, ir->letrec.nvars);
      eth_write_insn(tape, mkscp);

      // build body
      int ret = build(bldr, tape, ir->letrec.body, istc, e);

      // done
      end_logical_block(bldr, lbstart);
      return ret;
    }

    // TODO: collect similar code (see IF)
    // TODO: handle constants
    case ETH_IR_MATCH:
    {
      int expr = build(bldr, tape, ir->match.expr, false, e);

      if (ir->match.pat->tag == ETH_PATTERN_IDENT)
      { // optimize trivial identifier-match
        eth_ssa_pattern *pat = build_pattern(bldr, ir->match.pat, expr, NULL, e);
        eth_destroy_ssa_pattern(pat);
        return build(bldr, tape, ir->match.thenbr, istc, e);
      }
      else
      {
        int lbstart = begin_logical_block(bldr);
        // --
        int n1 = bldr->nssavals;
        match_result mchres;
        eth_ssa_pattern *pat = build_pattern(bldr, ir->match.pat, expr, &mchres, e);
        int n2 = bldr->nssavals;
        // --
        eth_ssa_tape *thentape = eth_create_ssa_tape();
        eth_insn *ctor = eth_insn_nop();
        ctor->flag |= ETH_IFLAG_NOBEFORE;
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

        eth_insn *insn = eth_insn_if_match(ret, expr, pat, thentape->head,
            elsetape->head);
        insn->iff.toplvl = ir->match.toplvl;
        insn->iff.likely = ir->match.likely;
        eth_destroy_ssa_tape(thentape);
        eth_destroy_ssa_tape(elsetape);

        for (int i = n1; i < n2; ++i)
        {
          if (not bldr->ssavinfo[i]->creatloc)
            bldr->ssavinfo[i]->creatloc = ctor;
        }

        eth_write_insn(tape, insn);
        if (ret >= 0)
          bldr->ssavinfo[ret]->creatloc = insn;

        return ret;
      }
    }

    case ETH_IR_ACCESS:
    {
      // XXX add exception handling
      int src = build(bldr, tape, ir->access.expr, false, e);
      int alt = ir->access.alt ? build(bldr, tape, ir->access.alt, false, e):-1;
      int out = new_val(bldr, RC_RULES_DEFAULT);
      eth_insn *insn = eth_insn_access(out, src, ir->access.fld, alt);
      bldr->ssavinfo[out]->creatloc = insn;
      eth_write_insn(tape, insn);
      return out;
    }

    case ETH_IR_MKRCRD:
    {
      int n = ir->mkrcrd.type->nfields;
      int vids[n];
      for (int i = 0; i < n; ++i)
        vids[i] = build(bldr, tape, ir->mkrcrd.fields[i], false, e);
      int out = new_val(bldr, RC_RULES_DEFAULT);
      eth_insn *insn = eth_insn_mkrcrd(out, vids, ir->mkrcrd.type);
      bldr->ssavinfo[out]->creatloc = insn;
      bldr->ssavinfo[out]->type = ir->mkrcrd.type;
      eth_write_insn(tape, insn);
      trace_mov(bldr, insn);
      return out;
    }

    case ETH_IR_UPDATE:
    {
      eth_use_symbol(update_error);
      int n = ir->update.n;
      int vids[n];
      int src = build(bldr, tape, ir->update.src, false, e);
      for (int i = 0; i < n; ++i)
        vids[i] = build(bldr, tape, ir->update.fields[i], false, e);
      int out = new_val(bldr, RC_RULES_PHI);
      eth_ssa_tape *errtape = eth_create_ssa_tape();
      int exnvid = new_val(bldr, RC_RULES_DISABLE);
      eth_t exn = eth_exn(update_error);
      eth_write_insn(errtape, eth_insn_cval(exnvid, exn));
      write_throw(bldr, errtape, exnvid, ir->loc);
      eth_insn *insn = eth_insn_if_update(out, src, vids, ir->update.ids, n,
          eth_insn_nop(), errtape->head);
      insn->iff.likely = 1;
      eth_destroy_ssa_tape(errtape);
      bldr->ssavinfo[out]->creatloc = insn;
      bldr->ssavinfo[out]->type = bldr->ssavinfo[src]->type;
      eth_write_insn(tape, insn);
      return out;
    }

    case ETH_IR_THROW:
    {
      int vid = build(bldr, tape, ir->throw.exn, false, e);
      write_throw(bldr, tape, vid, ir->loc);
      return vid;
    }

    case ETH_IR_RETURN:
    {
      int vid = build(bldr, tape, ir->retrn.expr, istc, e);
      eth_write_insn(tape, eth_insn_ret(vid));
      return vid;
    }

    case ETH_IR_MULTIMATCH:
    {
      abort();
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

    case ETH_INSN_LOOP:
      for (int i = 0; i < insn->loop.nargs; ++i)
      {
        if (insn->loop.args[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_IF:
      if (insn->iff.test == ETH_TEST_UPDATE)
      {
        for (int i = 0; i < insn->iff.update.n; ++i)
        {
          if (insn->iff.update.vids[i] == vid)
            return true;
        }
      }
      return insn->iff.cond == vid;

    case ETH_INSN_ACCESS:
      return insn->access.src == vid
          or insn->access.alt == vid;

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

    case ETH_INSN_MKSCP:
      for (int i = 0; i < insn->mkscp.nclos; ++i)
      {
        if (insn->mkscp.clos[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_POP:
      return false;

    case ETH_INSN_CAP:
      return false;

    case ETH_INSN_LDSCP:
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

    case ETH_INSN_LOOP:
      for (int i = 0; i < insn->loop.nargs; ++i)
      {
        if (insn->loop.args[i] == vid)
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

    case ETH_INSN_CATCH:
      return insn->catch.vid == vid;

    case ETH_INSN_MKRCRD:
      for (int i = 0; i < insn->mkrcrd.type->nfields; ++i)
      {
        if (insn->mkrcrd.vids[i] == vid)
          return true;
      }
      return false;

    case ETH_INSN_BINOP:
      if (insn->binop.op == ETH_CONS)
        return insn->binop.lhs == vid or insn->binop.rhs == vid;
      else
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

    case ETH_INSN_CATCH:
      *n = 1;
      return &insn->catch.vid;

    case ETH_INSN_MKRCRD:
      *n = insn->mkrcrd.type->nfields;
      return insn->mkrcrd.vids;

    case ETH_INSN_BINOP:
    {
      static int buf[2];
      buf[0] = insn->binop.lhs;
      buf[1] = insn->binop.rhs;
      *n = 2;
      return buf;
    }

    default:
      eth_error("wtf");
      abort();
  }
}

static inline bool
is_end(eth_insn *insn)
{
  return insn->tag == ETH_INSN_RET or
         insn->tag == ETH_INSN_CATCH or
         insn->tag == ETH_INSN_LOOP
  ;
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
    if (is_end(insn))
      return true;
  }

  return false;
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

      if (is_dead_end(b1))
      {
         if (not kill_value_T(kinfo, b1, vid))
          force_kill(kinfo, b1, vid);
      }
      else
        kill_value_F(kinfo, b1, vid);

      if (is_dead_end(b2))
      {
         if (not kill_value_T(kinfo, b2, vid))
          force_kill(kinfo, b2, vid);
      }
      else
        kill_value_F(kinfo, b2, vid);
    }

    if (insn->tag == ETH_INSN_TRY)
    {
      eth_insn *t = insn->try.trybr;
      eth_insn *c = insn->try.catchbr;

      if (is_dead_end(c))
      {
         if (not kill_value_T(kinfo, c, vid))
          force_kill(kinfo, c, vid);
      }
      else
        kill_value_F(kinfo, c, vid);
    }

    if (is_end(insn))
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
    if (is_dead_end(b1))
    {
       if (not kill_value_T(kinfo, b1, vid))
        force_kill(kinfo, b1, vid);
    }
    else
      kill_value_F(kinfo, b1, vid);

    if (is_dead_end(b2))
    {
       if (not kill_value_T(kinfo, b2, vid))
        force_kill(kinfo, b2, vid);
    }
    else
      kill_value_F(kinfo, b2, vid);

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

    if (is_end(insn))
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
  {
    eth_insn *insn = eth_insn_ref(vid);
    eth_insert_insn_after(begin, insn);
    eth_set_insn_comment(insn,
        "inserted by ssa-builder:insert_rc_default (kill_value_T() -> true)");
  }
  else
  {
    eth_insert_insn_after(begin, eth_insn_drop(vid));
  }
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
    // XXX: im not sure if mov==NULL is OK
    if (mov and not kill_value_T(kinfo, mov->next, vid))
    {
      eth_insn *unref = eth_insn_unref(vid);
      eth_insert_insn_after(mov, unref);
      add_killer(kinfo, vid, unref);
    }
    mov = find_mov(br2, vid);
    // XXX: im not sure if mov==NULL is OK
    if (mov and not kill_value_T(kinfo, mov->next, vid))
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

      if (bldr->ssavinfo[vid]->rules == RC_RULES_DISABLE)
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
            // Still do REF if move-semantics was already applied to this value
            // for this instruction
            doref = false;
            for (int j = i - 1; j >= 0; --j)
            {
              if (vids[j] == vid)
              {
                doref = true;
                break;
              }
            }
            break;
          }
        }
      }

      if (doref)
      {
        eth_insn *insn = eth_insn_ref(vid);
        eth_insert_insn_before(mov, insn);
        eth_set_insn_comment(insn, "inserted by ssa-builder:handle_movs");
      }
    }
  }
}

static void
insert_rc(ssa_builder *bldr, eth_insn *body)
{
  kill_info *kinfo = create_kill_info(bldr->nssavals);

  for (int vid = 0; vid < bldr->nssavals; ++vid)
  {
    ssa_value_info *ssavinfo = bldr->ssavinfo[vid];
    switch (ssavinfo->rules)
    {
      case RC_RULES_DEFAULT:
        assert(ssavinfo->creatloc);
        insert_rc_default(kinfo, ssavinfo->creatloc, vid);
        break;

      case RC_RULES_PHI:
        assert(ssavinfo->creatloc);
        insert_rc_phi(kinfo, ssavinfo->creatloc, vid);
        break;

      case RC_RULES_UNREF:
        assert(ssavinfo->creatloc);
        insert_rc_unref(kinfo, ssavinfo->creatloc, vid);
        break;

      case RC_RULES_DISABLE:
        break;
    }
  }
  handle_movs(bldr, kinfo);
  destroy_kill_info(kinfo);
}


static inline void
destroy_ssa(eth_ssa *ssa)
{
  eth_destroy_insn_list(ssa->body);
  free(ssa);
}

void
eth_ref_ssa(eth_ssa *ssa)
{
  ssa->rc += 1;
}

void
eth_unref_ssa(eth_ssa *ssa)
{
  if (--ssa->rc == 0)
    destroy_ssa(ssa);
}

void
eth_drop_ssa(eth_ssa *ssa)
{
  if (ssa->rc == 0)
    destroy_ssa(ssa);
}

static eth_ssa*
build_body(ssa_builder *bldr, eth_ssa_tape *tape, eth_ir_node *body, bool *e)
{
  int fnret = build_logical_block(bldr, tape, body, true, e);
  eth_write_insn(tape, eth_insn_ret(fnret));
  insert_rc(bldr, tape->head);
  return create_ssa(tape->head, bldr->nssavals, bldr->ntries);
}

static void
populate_capinfo(ssa_value_info *ssavinfo, const eth_capvar_info *capinfo)
{
  ssavinfo->type = capinfo->type;
  ssavinfo->cval = capinfo->cval;
}

eth_ssa*
eth_build_fn_body(eth_ir *fnbody, int arity, eth_capvar_info *caps, int ncaps,
    int *scpvars_local, int nscpvars, int selfscpidx, bool *e)
{
  ssa_builder *fnbldr = create_ssa_builder(fnbody->nvars);
  eth_ssa_tape *fntape = eth_create_ssa_tape();

  int capvars_local[ncaps];
  for (int i = 0; i < ncaps; ++i)
    capvars_local[i] = caps[i].varid_local;

  // declare arguments [0, arity):
  int args_local[arity];
  if (arity)
  {
    for (int i = 0; i < arity; ++i)
    {
      args_local[i] = new_val(fnbldr, RC_RULES_DEFAULT);
      fnbldr->irvinfo[i]->ssavid = args_local[i]; // varids of args are
                                                  // [0 ... arity)
                                                  // XXX so what?
    }

    eth_insn *pop = eth_insn_pop(args_local, arity);
    eth_write_insn(fntape, pop);

    for (int i = 0; i < arity; ++i)
      fnbldr->ssavinfo[args_local[i]]->creatloc = pop;
  }

  // prepare loop entry point
  eth_insn *last_before_loop = NULL;

  // declare captures [arity, arity + ncap):
  int capvids_local[ncaps];
  if (ncaps > 0)
  {
    for (int i = 0; i < ncaps; ++i)
    {
      capvids_local[i] = new_val(fnbldr, RC_RULES_DISABLE);
      fnbldr->irvinfo[capvars_local[i]]->ssavid = capvids_local[i];
      populate_capinfo(fnbldr->ssavinfo[capvids_local[i]], &caps[i]);
    }
    eth_write_insn(fntape,
        last_before_loop = eth_insn_cap(capvids_local, ncaps));
  }

  // declare scope vars [arity + ncap, arity + ncap + nscp):
  int scpvids_local[nscpvars];
  if (nscpvars > 0)
  {
    for (int i = 0; i < nscpvars; ++i)
    {
      scpvids_local[i] = new_val(fnbldr, RC_RULES_DISABLE);
      fnbldr->irvinfo[scpvars_local[i]]->ssavid = scpvids_local[i];
      fnbldr->ssavinfo[scpvids_local[i]]->type = eth_function_type;
    }
    // mark self for loop-detection:
    if (selfscpidx >= 0)
    {
      int selfvid_local = scpvids_local[selfscpidx];
      fnbldr->ssavinfo[selfvid_local]->isthis = true;
    }
    // LDSCP
    eth_write_insn(fntape,
        last_before_loop = eth_insn_ldscp(scpvids_local, nscpvars));
  }

  if (last_before_loop)
  {
    // move ctor-instruction of arguments:
    for (int i = 0; i < arity; ++i)
      fnbldr->ssavinfo[args_local[i]]->creatloc = last_before_loop;
  }

  // build body and set up loop entry point (if has captures or scope-vars):
  eth_ssa *ssa = build_body(fnbldr, fntape, fnbody->body, e);
  // move entry point to after CAP- and LDSCP- instructions
  if (last_before_loop)
    last_before_loop->next->flag |= ETH_IFLAG_ENTRYPOINT;

  eth_destroy_ssa_tape(fntape);
  destroy_ssa_builder(fnbldr);

  return ssa;
}


typedef struct {
  int ndefs;
} module_info;

static eth_t
make_module(void)
{
  module_info *data = eth_this->proc.data;
  eth_t ret = eth_sp[0];
  eth_t acc = eth_nil;
  int n = data->ndefs;
  for (int i = n - 1; i >= 0; --i)
    acc = eth_cons(eth_sp[i+1], acc);
  eth_pop_stack(n + 1);
  return eth_tup2(ret, acc);
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

  // create build-context and an SSA-tape to write to
  ssa_builder *bldr = create_ssa_builder(ir->nvars);
  eth_ssa_tape *tape = eth_create_ssa_tape();
  int lbstart = begin_logical_block(bldr);
  // add specializations to the build-context
  for (int i = 0; i < ir->nspecs; ++i)
  {
    eth_spec *spec = ir->specs[i];
    switch (spec->tag)
    {
      case ETH_SPEC_TYPE:
        bldr->irvinfo[spec->type_spec.varid]->type = spec->type_spec.type;
        break;
    }
  }
  int ret = build(bldr, tape, ir->body, false, &e);
  end_logical_block(bldr, lbstart);

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

      eth_t mkmod = eth_create_proc(make_module, n + 1, data, (void*)free);

      int modret = new_val(bldr, RC_RULES_DEFAULT);
      int fnvid = new_val(bldr, RC_RULES_DISABLE);
      eth_write_insn(tlvltape, eth_insn_cval(fnvid, mkmod));
      int argvids[n + 1];
      argvids[0] = ret;
      for (int i = 0; i < n; ++i)
      {
        switch (defs->defs[i].tag)
        {
          case ETH_IRDEF_VAR:
            argvids[i+1] = bldr->irvinfo[defs->defs[i].varid]->ssavid;
            break;

          case ETH_IRDEF_CVAL:
          {
            int vid = new_val(bldr, RC_RULES_DISABLE);
            eth_write_insn(tlvltape, eth_insn_cval(vid, defs->defs[i].cval));
            argvids[i+1] = vid;
            break;
          }
        }
      }

      eth_insn *insn = eth_insn_apply(modret, fnvid, argvids, n + 1);
      eth_write_insn(tlvltape, insn);
      bldr->ssavinfo[modret]->creatloc = insn;

      eth_write_insn(tlvltape, eth_insn_ret(modret));

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
    ssa = create_ssa(tape->head, bldr->nssavals, bldr->ntries);
  }
  eth_destroy_ssa_tape(tape);
  destroy_ssa_builder(bldr);

  return ssa;
}

