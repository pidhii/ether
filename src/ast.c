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
        free(ast->let.nams[i]);
        eth_unref_ast(ast->let.vals[i]);
      }
      free(ast->let.nams);
      free(ast->let.vals);
      eth_unref_ast(ast->let.body);
      break;

    case ETH_AST_BINOP:
      eth_unref_ast(ast->binop.lhs);
      eth_unref_ast(ast->binop.rhs);
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
eth_ast_let(char *const *nams, eth_ast *const *vals, int n, eth_ast *body)
{
  eth_ast *ast = create_ast_node(ETH_AST_LET);
  eth_ref_ast(ast->let.body = body);
  ast->let.n = n;
  ast->let.nams = malloc(sizeof(char*) * n);
  ast->let.vals = malloc(sizeof(eth_ast*) * n);
  for (int i = 0; i < n; ++i)
  {
    eth_ref_ast(ast->let.vals[i] = vals[i]);
    ast->let.nams[i] = strdup(nams[i]);
  }
  return ast;
}

eth_ast*
eth_ast_letrec(char *const *nams, eth_ast *const *vals, int n, eth_ast *body)
{
  eth_ast *ast = eth_ast_let(nams, vals, n, body);
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
  return ast;
}
