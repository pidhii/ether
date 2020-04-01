#include "ether/ether.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

eth_ast_pattern*
eth_ast_ident_pattern(const char *ident)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->tag = ETH_PATTERN_IDENT;
  pat->ident.str = strdup(ident);
  pat->ident.pub = false;
  return pat;
}

eth_ast_pattern*
eth_ast_unpack_pattern(const char *type, char *const fields[],
    eth_ast_pattern *pats[], int n)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->tag = ETH_PATTERN_UNPACK;
  pat->unpack.type = strdup(type);
  pat->unpack.fields = malloc(sizeof(char*) * n);
  pat->unpack.subpats = malloc(sizeof(eth_ast_pattern*) * n);
  pat->unpack.n = n;
  for (int i = 0; i < n; ++i)
    pat->unpack.fields[i] = strdup(fields[i]);
  memcpy(pat->unpack.subpats, pats, sizeof(eth_ast_pattern*) * n);
  return pat;
}

eth_ast_pattern*
eth_ast_symbol_pattern(eth_t sym)
{
  eth_ast_pattern *pat = malloc(sizeof(eth_ast_pattern));
  pat->tag = ETH_PATTERN_SYMBOL;
  eth_ref(pat->symbol.sym = sym);
  return pat;
}

void
eth_destroy_ast_pattern(eth_ast_pattern *pat)
{
  switch (pat->tag)
  {
    case ETH_PATTERN_IDENT:
      free(pat->ident.str);
      break;

    case ETH_PATTERN_UNPACK:
      for (int i = 0; i < pat->unpack.n; ++i)
      {
        free(pat->unpack.fields[i]);
        eth_destroy_ast_pattern(pat->unpack.subpats[i]);
      }
      free(pat->unpack.type);
      free(pat->unpack.fields);
      free(pat->unpack.subpats);
      break;

    case ETH_PATTERN_SYMBOL:
      eth_unref(pat->symbol.sym);
      break;
  }
  free(pat);
}


static inline eth_ast*
create_ast_node(eth_ast_tag tag)
{
  eth_ast *node = malloc(sizeof(eth_ast));
  node->rc = 0;
  node->tag = tag;
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
        eth_destroy_ast_pattern(ast->let.pats[i]);
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
        free(ast->fn.args[i]);
      free(ast->fn.args);
      eth_unref_ast(ast->fn.body);
      break;

    case ETH_AST_MATCH:
      eth_destroy_ast_pattern(ast->match.pat);
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
      eth_destroy_ast_pattern(ast->try.pat);
      eth_unref_ast(ast->try.trybr);
      eth_unref_ast(ast->try.catchbr);
      break;
  }

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
    ast->let.pats[i] = pats[i];
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
eth_ast_fn(char **args, int arity, eth_ast *body)
{
  eth_ast *ast = create_ast_node(ETH_AST_FN);
  ast->fn.args = malloc(sizeof(char*) * arity);
  ast->fn.arity = arity;
  ast->fn.body = body;
  for (int i = 0; i < arity; ++i)
    ast->fn.args[i] = strdup(args[i]);
  eth_ref_ast(body);
  return ast;
}

eth_ast*
eth_ast_match(eth_ast_pattern *pat, eth_ast *expr, eth_ast *thenbr,
    eth_ast *elsebr)
{
  eth_ast *ast = create_ast_node(ETH_AST_MATCH);
  ast->match.pat = pat;
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
  eth_ast *ast = create_ast_node(ETH_AST_TRY);
  ast->try.pat = pat ? pat : eth_ast_ident_pattern("");
  eth_ref_ast(ast->try.trybr = try);
  eth_ref_ast(ast->try.catchbr = catch);
  ast->try.likely = likely;
  return ast;
}

