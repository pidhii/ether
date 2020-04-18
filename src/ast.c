#include "ether/ether.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>


ETH_MODULE("ether:ast")


eth_ast_pattern*
eth_ast_dummy_pattern(void)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_PATTERN_DUMMY;
  return pat;
}

eth_ast_pattern*
eth_ast_ident_pattern(const char *ident)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_PATTERN_IDENT;
  pat->ident.str = strdup(ident);
  pat->ident.attr = 0;
  return pat;
}

eth_ast_pattern*
eth_ast_unpack_pattern(const char *type, char *const fields[],
    eth_ast_pattern *pats[], int n)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_PATTERN_UNPACK;
  pat->unpack.isctype = false;
  pat->unpack.type.str = strdup(type);
  pat->unpack.fields = malloc(sizeof(char*) * n);
  pat->unpack.subpats = malloc(sizeof(eth_ast_pattern*) * n);
  pat->unpack.n = n;
  pat->unpack.alias = NULL;
  for (int i = 0; i < n; ++i)
  {
    pat->unpack.fields[i] = strdup(fields[i]);
    eth_ref_ast_pattern(pat->unpack.subpats[i] = pats[i]);
  }
  return pat;
}

eth_ast_pattern*
eth_ast_unpack_pattern_with_type(eth_type *type, char *const fields[],
    eth_ast_pattern *pats[], int n)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_PATTERN_UNPACK;
  pat->unpack.isctype = true;
  pat->unpack.type.ctype = type;
  pat->unpack.fields = malloc(sizeof(char*) * n);
  pat->unpack.subpats = malloc(sizeof(eth_ast_pattern*) * n);
  pat->unpack.n = n;
  pat->unpack.alias = NULL;
  for (int i = 0; i < n; ++i)
  {
    pat->unpack.fields[i] = strdup(fields[i]);
    eth_ref_ast_pattern(pat->unpack.subpats[i] = pats[i]);
  }
  return pat;
}

eth_ast_pattern*
eth_ast_record_pattern(char *const fields[], eth_ast_pattern *pats[], int n)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_PATTERN_RECORD;
  pat->record.fields = malloc(sizeof(char*) * n);
  pat->record.subpats = malloc(sizeof(eth_ast_pattern*) * n);
  pat->record.n = n;
  pat->record.alias = NULL;
  for (int i = 0; i < n; ++i)
  {
    pat->record.fields[i] = strdup(fields[i]);
    eth_ref_ast_pattern(pat->record.subpats[i] = pats[i]);
  }
  return pat;
}

eth_ast_pattern*
eth_ast_constant_pattern(eth_t val)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->rc = 0;
  pat->tag = ETH_PATTERN_CONSTANT;
  eth_ref(pat->constant.val = val);
  return pat;
}

void
eth_set_pattern_alias(eth_ast_pattern *pat, const char *alias)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_UNPACK:
      assert(pat->unpack.alias == NULL);
      pat->unpack.alias = strdup(alias);
      break;

    case ETH_PATTERN_RECORD:
      assert(pat->record.alias == NULL);
      pat->record.alias = strdup(alias);
      break;

    default:
      eth_error("aliases are only available for UNPACK and RECORD patterns");
      abort();
  }
}

static void
destroy_ast_pattern(eth_ast_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_DUMMY:
      break;

    case ETH_PATTERN_IDENT:
      free(pat->ident.str);
      break;

    case ETH_PATTERN_UNPACK:
      for (int i = 0; i < pat->unpack.n; ++i)
      {
        free(pat->unpack.fields[i]);
        eth_unref_ast_pattern(pat->unpack.subpats[i]);
      }
      if (not pat->unpack.isctype)
        free(pat->unpack.type.str);
      free(pat->unpack.fields);
      free(pat->unpack.subpats);
      if (pat->unpack.alias)
        free(pat->unpack.alias);
      break;

    case ETH_PATTERN_CONSTANT:
      eth_unref(pat->constant.val);
      break;

    case ETH_PATTERN_RECORD:
      for (int i = 0; i < pat->record.n; ++i)
      {
        free(pat->record.fields[i]);
        eth_unref_ast_pattern(pat->record.subpats[i]);
      }
      free(pat->record.fields);
      free(pat->record.subpats);
      if (pat->record.alias)
        free(pat->record.alias);
      break;
  }
  free(pat);
}

void
eth_ref_ast_pattern(eth_ast_pattern *pat)
{
  pat->rc += 1;
}

void
eth_unref_ast_pattern(eth_ast_pattern *pat)
{
  if (--pat->rc == 0)
    destroy_ast_pattern(pat);
}

void
eth_drop_ast_pattern(eth_ast_pattern *pat)
{
  if (pat->rc == 0)
    destroy_ast_pattern(pat);
}

static inline eth_ast*
create_ast_node(eth_ast_tag tag)
{
  eth_ast *node = malloc(sizeof(eth_ast));
  node->rc = 0;
  node->tag = tag;
  node->loc = NULL;
  return node;
}

static inline void
destroy_ast_node(eth_ast *ast)
{
  if (ast == NULL)
    return;

  switch (ast->tag)
  {
    case ETH_AST_CVAL:
      eth_unref(ast->cval.val);
      break;

    case ETH_AST_IDENT:
      free(ast->ident.str);
      break;

    case ETH_AST_APPLY:
      eth_unref_ast(ast->apply.fn);
      for (int i = 0; i < ast->apply.nargs; ++i)
        eth_unref_ast(ast->apply.args[i]);
      free(ast->apply.args);
      break;

    case ETH_AST_IF:
      eth_unref_ast(ast->iff.cond);
      eth_unref_ast(ast->iff.then);
      eth_unref_ast(ast->iff.els);
      break;

    case ETH_AST_SEQ:
      eth_unref_ast(ast->seq.e1);
      eth_unref_ast(ast->seq.e2);
      break;

    case ETH_AST_LET:
    case ETH_AST_LETREC:
      for (int i = 0; i < ast->let.n; ++i)
      {
        eth_unref_ast(ast->let.vals[i]);
        eth_unref_ast_pattern(ast->let.pats[i]);
      }
      free(ast->let.pats);
      free(ast->let.vals);
      eth_unref_ast(ast->let.body);
      break;

    case ETH_AST_BINOP:
      eth_unref_ast(ast->binop.lhs);
      eth_unref_ast(ast->binop.rhs);
      break;

    case ETH_AST_UNOP:
      eth_unref_ast(ast->unop.expr);
      break;

    case ETH_AST_FN:
      for (int i = 0; i < ast->fn.arity; ++i)
        eth_unref_ast_pattern(ast->fn.args[i]);
      free(ast->fn.args);
      eth_unref_ast(ast->fn.body);
      break;

    case ETH_AST_MATCH:
      eth_unref_ast_pattern(ast->match.pat);
      eth_unref_ast(ast->match.expr);
      eth_unref_ast(ast->match.thenbr);
      eth_unref_ast(ast->match.elsebr);
      break;

    case ETH_AST_IMPORT:
      free(ast->import.module);
      if (ast->import.alias)
        free(ast->import.alias);
      if (ast->import.nams)
      {
        for (int i = 0; i < ast->import.nnam; ++i)
          free(ast->import.nams[i]);
        free(ast->import.nams);
      }
      eth_unref_ast(ast->import.body);
      break;

    case ETH_AST_AND:
    case ETH_AST_OR:
      eth_unref_ast(ast->scor.lhs);
      eth_unref_ast(ast->scor.rhs);
      break;

    case ETH_AST_TRY:
      eth_unref_ast_pattern(ast->try.pat);
      eth_unref_ast(ast->try.trybr);
      eth_unref_ast(ast->try.catchbr);
      break;

    case ETH_AST_MKRCRD:
      if (not ast->mkrcrd.isctype)
        free(ast->mkrcrd.type.str);
      for (int i = 0; i < ast->mkrcrd.n; ++i)
      {
        free(ast->mkrcrd.fields[i]);
        eth_unref_ast(ast->mkrcrd.vals[i]);
      }
      free(ast->mkrcrd.fields);
      free(ast->mkrcrd.vals);
      break;

    case ETH_AST_MULTIMATCH:
      for (int i = 0; i < ast->multimatch.table->h; ++i)
        eth_unref_ast(ast->multimatch.exprs[i]);
      free(ast->multimatch.exprs);
      eth_destroy_match_table(ast->multimatch.table);
      break;
  }

  if (ast->loc)
    eth_unref_location(ast->loc);
  free(ast);
}

extern inline void
eth_ref_ast(eth_ast *ast)
{
  ast->rc += 1;
}

extern inline void
eth_unref_ast(eth_ast *ast)
{
  if (--ast->rc == 0)
    destroy_ast_node(ast);
}

extern inline void
eth_drop_ast(eth_ast *ast)
{
  if (ast->rc == 0)
    destroy_ast_node(ast);
}

void
eth_set_ast_location(eth_ast *ast, eth_location *loc)
{
  if (ast->loc)
    eth_unref_location(ast->loc);
  eth_ref_location(ast->loc = loc);
}

eth_ast*
eth_ast_cval(eth_t val)
{
  eth_ast *ast = create_ast_node(ETH_AST_CVAL);
  eth_ref(ast->cval.val = val);
  return ast;
}

eth_ast*
eth_ast_ident(const char *ident)
{
  eth_ast *ast = create_ast_node(ETH_AST_IDENT);
  ast->ident.str = strdup(ident);
  return ast;
}

eth_ast*
eth_ast_apply(eth_ast *fn, eth_ast *const *args, int nargs)
{
  eth_ast *ast = create_ast_node(ETH_AST_APPLY);
  eth_ref_ast(ast->apply.fn = fn);
  ast->apply.nargs = nargs;
  ast->apply.args = malloc(sizeof(eth_ast*) * nargs);
  for (int i = 0; i < nargs; ++i)
    eth_ref_ast(ast->apply.args[i] = args[i]);
  return ast;
}

void
eth_ast_append_arg(eth_ast *ast, eth_ast *arg)
{
  assert(ast->tag == ETH_AST_APPLY);
  int nargs = ast->apply.nargs + 1;
  ast->apply.args = realloc(ast->apply.args, sizeof(eth_ast*) * (nargs + 1));
  ast->apply.args[ast->apply.nargs++] = arg;
  eth_ref_ast(arg);
}

eth_ast*
eth_ast_if(eth_ast *cond, eth_ast *then, eth_ast *els)
{
  eth_ast *ast = create_ast_node(ETH_AST_IF);
  eth_ref_ast(ast->iff.cond = cond);
  eth_ref_ast(ast->iff.then = then);
  eth_ref_ast(ast->iff.els  = els );
  ast->iff.toplvl = ETH_TOPLVL_NONE;
  return ast;
}

eth_ast*
eth_ast_seq(eth_ast *e1, eth_ast *e2)
{
  eth_ast *ast = create_ast_node(ETH_AST_SEQ);
  eth_ref_ast(ast->seq.e1 = e1);
  eth_ref_ast(ast->seq.e2 = e2);
  return ast;
}

eth_ast*
eth_ast_let(eth_ast_pattern *const pats[], eth_ast *const *vals, int n,
    eth_ast *body)
{
  eth_ast *ast = create_ast_node(ETH_AST_LET);
  eth_ref_ast(ast->let.body = body);
  ast->let.n = n;
  ast->let.pats = malloc(sizeof(eth_ast_pattern*) * n);
  ast->let.vals = malloc(sizeof(eth_ast*) * n);
  for (int i = 0; i < n; ++i)
  {
    eth_ref_ast(ast->let.vals[i] = vals[i]);
    eth_ref_ast_pattern(ast->let.pats[i] = pats[i]);
  }
  return ast;
}

eth_ast*
eth_ast_letrec(eth_ast_pattern *const pats[], eth_ast *const *vals, int n,
    eth_ast *body)
{
  eth_ast *ast = eth_ast_let(pats, vals, n, body);
  ast->tag = ETH_AST_LETREC;
  return ast;
}

eth_ast*
eth_ast_binop(eth_binop op, eth_ast *lhs, eth_ast *rhs)
{
  eth_ast *ast = create_ast_node(ETH_AST_BINOP);
  ast->binop.op = op;
  eth_ref_ast(ast->binop.lhs = lhs);
  eth_ref_ast(ast->binop.rhs = rhs);
  return ast;
}

eth_ast*
eth_ast_unop(eth_unop op, eth_ast *expr)
{
  eth_ast *ast = create_ast_node(ETH_AST_UNOP);
  ast->unop.op = op;
  eth_ref_ast(ast->unop.expr = expr);
  return ast;
}

eth_ast*
eth_ast_fn_with_patterns(eth_ast_pattern *const args[], int arity, eth_ast *body)
{
  eth_ast *ast = create_ast_node(ETH_AST_FN);
  ast->fn.args = malloc(sizeof(eth_ast_pattern*) * arity);
  ast->fn.arity = arity;
  ast->fn.body = body;
  for (int i = 0; i < arity; ++i)
    eth_ref_ast_pattern(ast->fn.args[i] = args[i]);
  eth_ref_ast(body);
  return ast;
}

eth_ast*
eth_ast_fn(char **args, int arity, eth_ast *body)
{
  eth_ast_pattern *pats[arity];
  for (int i = 0; i < arity; ++i)
    pats[i] = eth_ast_ident_pattern(args[i]);
  return eth_ast_fn_with_patterns(pats, arity, body);
}

eth_ast*
eth_ast_match(eth_ast_pattern *pat, eth_ast *expr, eth_ast *thenbr,
    eth_ast *elsebr)
{
  eth_ast *ast = create_ast_node(ETH_AST_MATCH);
  ast->match.pat = pat;
  eth_ref_ast_pattern(pat);
  eth_ref_ast(ast->match.expr = expr);
  eth_ref_ast(ast->match.thenbr = thenbr);
  eth_ref_ast(ast->match.elsebr = elsebr);
  ast->match.toplvl = ETH_TOPLVL_NONE;
  return ast;
}

eth_ast*
eth_ast_import(const char *module, const char *alias, char *const nams[],
    int nnam, eth_ast *body)
{
  eth_ast *ast = create_ast_node(ETH_AST_IMPORT);
  ast->import.module = strdup(module);
  eth_ref_ast(ast->import.body = body);
  ast->import.alias = alias ? strdup(alias) : NULL;
  if (nams)
  {
    ast->import.nams = malloc(sizeof(char*) * nnam);
    ast->import.nnam = nnam;
    for (int i = 0; i < nnam; ++i)
      ast->import.nams[i] = strdup(nams[i]);
  }
  else
    ast->import.nams = NULL;
  return ast;
}

eth_ast*
eth_ast_and(eth_ast *lhs, eth_ast *rhs)
{
  eth_ast *ast = create_ast_node(ETH_AST_AND);
  eth_ref_ast(ast->scand.lhs = lhs);
  eth_ref_ast(ast->scand.rhs = rhs);
  return ast;
}

eth_ast*
eth_ast_or(eth_ast *lhs, eth_ast *rhs)
{
  eth_ast *ast = create_ast_node(ETH_AST_OR);
  eth_ref_ast(ast->scor.lhs = lhs);
  eth_ref_ast(ast->scor.rhs = rhs);
  return ast;
}

eth_ast*
eth_ast_try(eth_ast_pattern *pat, eth_ast *try, eth_ast *catch, int likely)
{
  // inner pattern must non-dummy because we will need that value
  // to test for exit-object
  if (pat == NULL)
    pat = eth_ast_ident_pattern("");
  else if (pat->tag == ETH_PATTERN_DUMMY)
  {
    eth_drop_ast_pattern(pat);
    pat = eth_ast_ident_pattern("");
  }
  bool check_exit = eth_is_wildcard(pat->tag);

  // use exception-pattern so that user is actually matching with `what'
  char *what = "what";
  pat = eth_ast_unpack_pattern_with_type(eth_exception_type, &what, &pat, 1);

  eth_ast *ast = create_ast_node(ETH_AST_TRY);
  ast->try.pat = pat;
  eth_ref_ast_pattern(ast->try.pat);
  eth_ref_ast(ast->try.trybr = try);
  eth_ref_ast(ast->try.catchbr = catch);
  ast->try.likely = likely;
  ast->try._check_exit = check_exit;
  return ast;
}

eth_ast*
eth_ast_make_record(const char *type, char *const fields[],
    eth_ast *const vals[], int n)
{
  eth_ast *ast = create_ast_node(ETH_AST_MKRCRD);
  ast->mkrcrd.isctype = false;
  ast->mkrcrd.type.str = strdup(type);
  ast->mkrcrd.fields = malloc(sizeof(char*) * n);
  ast->mkrcrd.vals = malloc(sizeof(eth_ast*) * n);
  ast->mkrcrd.n = n;
  for (int i = 0; i < n; ++i)
  {
    eth_ref_ast(ast->mkrcrd.vals[i] = vals[i]);
    ast->mkrcrd.fields[i] = strdup(fields[i]);
  }
  return ast;
}

eth_ast*
eth_ast_make_record_with_type(eth_type *type, char *const fields[],
   eth_ast *const vals[], int n)
{
  eth_ast *ast = create_ast_node(ETH_AST_MKRCRD);
  ast->mkrcrd.isctype = true;
  ast->mkrcrd.type.ctype = type;
  ast->mkrcrd.fields = malloc(sizeof(char*) * n);
  ast->mkrcrd.vals = malloc(sizeof(eth_ast*) * n);
  ast->mkrcrd.n = n;
  for (int i = 0; i < n; ++i)
  {
    eth_ref_ast(ast->mkrcrd.vals[i] = vals[i]);
    ast->mkrcrd.fields[i] = strdup(fields[i]);
  }
  return ast;
}

eth_match_table*
eth_create_match_table(eth_ast_pattern **const tab[], eth_ast *const exprs[],
    int h, int w)
{
  eth_match_table *table = malloc(sizeof(eth_match_table));
  table->tab = malloc(sizeof(eth_ast_pattern*) * h);
  for (int i = 0; i < h; ++i)
  {
    table->tab[i] = malloc(sizeof(eth_ast_pattern*) * w);
    for (int j = 0; j < w; ++j)
      eth_ref_ast_pattern(table->tab[i][j] = tab[i][j]);
  }
  table->exprs = malloc(sizeof(eth_ast*) * h);
  for (int i = 0; i < h; ++i)
    eth_ref_ast(table->exprs[i] = exprs[i]);
  table->w = w;
  table->h = h;
  return table;
}

void
eth_destroy_match_table(eth_match_table *table)
{
  for (int i = 0; i < table->w; ++i)
  {
    for (int j = 0; j < table->h; ++j)
      eth_unref_ast_pattern(table->tab[i][j]);
    free(table->tab[i]);
  }
  free(table->tab);
  for (int j = 0; j < table->h; ++j)
    eth_unref_ast(table->exprs[j]);
  free(table->exprs);
  free(table);
}

eth_ast*
eth_ast_multimatch(eth_match_table *table, eth_ast *const exprs[])
{
  eth_ast *ast = create_ast_node(ETH_AST_MULTIMATCH);
  ast->multimatch.table = table;
  ast->multimatch.exprs = malloc(sizeof(eth_ast*) * table->w);
  for (int i = 0; i < table->w; ++i)
    eth_ref_ast(ast->multimatch.exprs[i] = exprs[i]);
  return ast;
}

