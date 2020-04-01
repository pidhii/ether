%define parse.error verbose
%define api.pure true
%param {eth_scanner *yyscanner}
%locations

%{
/*#define YYDEBUG 1*/

#include "ether/ether.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>


ETH_MODULE("ether:parser")


extern
int yylex();

extern void
yyset_in(FILE *in, eth_scanner *scanner);

extern FILE*
yyget_in(eth_scanner *scanner);

static
void yyerror(void *locp, eth_scanner *yyscanner, const char *what);

static eth_ast* g_result;
static bool g_iserror;
int _eth_start_token = -1;
%}

%code requires { #include "codeine/vec.h" }

%union {
  eth_ast *ast;
  eth_number_t number;
  eth_t constant;
  char *string;
  char character;
  bool boolean;
  struct {
    cod_vec(eth_ast_pattern*) pats;
    cod_vec(eth_ast*) vals;
  } binds;
  struct {
    eth_ast_pattern *pat;
    eth_ast *val;
  } bind;
  cod_vec(eth_ast*) astvec;
  cod_vec(char*) strvec;
  cod_vec(char) charvec;
  eth_ast_pattern *pattern;
}

%destructor {
  eth_drop_ast($$);
} <ast>

%destructor {
  eth_drop($$);
} <constant>

%destructor {
  free($$);
} <string>

%destructor {
  eth_destroy_ast_pattern($$.pat);
  eth_drop_ast($$.val);
} <bind>

%destructor {
  cod_vec_iter($$.pats, i, x, eth_destroy_ast_pattern(x));
  cod_vec_iter($$.vals, i, x, eth_drop_ast(x));
  cod_vec_destroy($$.pats);
  cod_vec_destroy($$.vals);
} <binds>

%destructor {
  cod_vec_iter($$, i, x, eth_drop_ast(x));
  cod_vec_destroy($$);
} <astvec>

%destructor {
  if ($$.data)
  {
    cod_vec_iter($$, i, x, free(x));
    cod_vec_destroy($$);
  }
} <strvec>

%destructor {
  cod_vec_destroy($$);
} <charvec>

%destructor {
  eth_destroy_ast_pattern($$);
} <pattern>


// =============================================================================
%token UNDEFINED
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%token<number> NUMBER
%token<string> SYMBOL CAPSYMBOL
%token<constant> CONST
%token<character> CHAR
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%nonassoc LET REC AND IN FN IFLET WHENLET
%nonassoc PUB IMPORT AS UNQUALIFIED
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%right RARROW
%right ';'
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%nonassoc IF THEN ELSE WHEN UNLESS TRY WITH
%right OR
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// level 0:
%right '$'
// level 1:
%right OPOR
// level 2:
%right OPAND
// level 3:
%nonassoc '<' LE '>' GE EQ NE IS
// level 4:
%right ':' PPLUS
// level 5:
%left '+' '-'
// level 6:
%left '*' '/' '%' MOD LAND LOR LXOR
// level 7:
%right '^' LSHL LSHR ASHL ASHR
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%left UMINUS UPLUS NOT LNOT
%nonassoc '!'


// =============================================================================
%type<ast> atom
%type<ast> form
%type<ast> expr
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%type<string> ident
/*%type<string> capident*/
%type<binds> binds
%type<bind> bind
%type<boolean> maybe_pub
%type<astvec> args
%type<strvec> fnargs
%type<string> string
%type<charvec> string_aux
%type<pattern> atomic_pattern pattern
%type<strvec> maybe_imports import_list
%type<astvec> list
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%start entry


%%

entry
  : expr { g_result = $1; }
;

atom
  : NUMBER { $$ = eth_ast_cval(eth_create_number($1)); }
  | ident { $$ = eth_ast_ident($1); free($1); }
  | CONST  { $$ = eth_ast_cval($1); }
  | string { $$ = eth_ast_cval(eth_create_string($1)); free($1); }
  | CAPSYMBOL { $$ = eth_ast_cval(eth_sym($1)); free($1); }
  | '('')' { $$ = eth_ast_cval(eth_nil); }
  | '(' expr ')' { $$ = $2; }
  | atom '!' { $$ = eth_ast_apply($1, NULL, 0); }
  | '[' list ']' {
    $$ = eth_ast_cval(eth_nil);
    cod_vec_riter($2, i, x, $$ = eth_ast_binop(ETH_CONS, x, $$));
    cod_vec_destroy($2);
  }
  | '['']' { $$ = eth_ast_cval(eth_nil); }
;

form
  : atom
  | atom args {
    $$ = eth_ast_apply($1, $2.data, $2.len);
    cod_vec_destroy($2);
  }
  | atom '$' expr {
    $$ = eth_ast_apply($1, &$3, 1);
  }
  | atom args '$' expr {
    cod_vec_push($2, $4);
    $$ = eth_ast_apply($1, $2.data, $2.len);
    cod_vec_destroy($2);
  }
;

expr
  : form
  | IMPORT CAPSYMBOL maybe_imports IN expr {
    $$ = eth_ast_import($2, NULL, $3.data, $3.len, $5);
    free($2);
    if ($3.data)
    {
      cod_vec_iter($3, i, x, free(x));
      cod_vec_destroy($3);
    }
  }
  | IMPORT CAPSYMBOL AS CAPSYMBOL maybe_imports IN expr {
    $$ = eth_ast_import($2, $4, $5.data, $5.len, $7);
    if ($5.data)
    {
      cod_vec_iter($5, i, x, free(x));
      cod_vec_destroy($5);
    }
    free($2);
    free($4);
  }
  | IMPORT UNQUALIFIED CAPSYMBOL maybe_imports IN expr {
    $$ = eth_ast_import($3, "", $4.data, $4.len, $6);
    if ($4.data)
    {
      cod_vec_iter($4, i, x, free(x));
      cod_vec_destroy($4);
    }
    free($3);
  }
  | TRY expr WITH pattern RARROW expr %prec WITH {
    $$ = eth_ast_try($4, $2, $6, 1);
  }
  | expr OR expr { $$ = eth_ast_try(NULL, $1, $3, 0); }
  | LET binds IN expr {
    $$ = eth_ast_let($2.pats.data, $2.vals.data, $2.pats.len, $4);
    cod_vec_destroy($2.pats);
    cod_vec_destroy($2.vals);
  }
  | LET REC binds IN expr {
    $$ = eth_ast_letrec($3.pats.data, $3.vals.data, $3.pats.len, $5);
    cod_vec_destroy($3.pats);
    cod_vec_destroy($3.vals);
  }
  | IFLET pattern '=' expr THEN expr ELSE expr {
    $$ = eth_ast_match($2, $4, $6, $8);
  }
  | WHENLET pattern '=' expr THEN expr {
    $$ = eth_ast_match($2, $4, $6, eth_ast_cval(eth_nil));
  }
  | IF expr THEN expr ELSE expr { $$ = eth_ast_if($2, $4, $6); }
  | WHEN expr THEN expr { $$ = eth_ast_if($2, $4, eth_ast_cval(eth_nil)); }
  | UNLESS expr THEN expr { $$ = eth_ast_if($2, eth_ast_cval(eth_nil), $4); }
  | FN fnargs RARROW expr {
    $$ = eth_ast_fn($2.data, $2.len, $4);
    cod_vec_iter($2, i, x, free(x));
    cod_vec_destroy($2);
  }
  | expr ';'        { $$ = $1; }
  | expr OPAND expr { $$ = eth_ast_and($1, $3); }
  | expr OPOR  expr { $$ = eth_ast_or($1, $3); }
  | expr ';'   expr { $$ = eth_ast_seq($1, $3); }
  | expr '+'   expr { $$ = eth_ast_binop(ETH_ADD , $1, $3); }
  | expr '-'   expr { $$ = eth_ast_binop(ETH_SUB , $1, $3); }
  | expr '*'   expr { $$ = eth_ast_binop(ETH_MUL , $1, $3); }
  | expr '/'   expr { $$ = eth_ast_binop(ETH_DIV , $1, $3); }
  | expr '<'   expr { $$ = eth_ast_binop(ETH_LT  , $1, $3); }
  | expr LE    expr { $$ = eth_ast_binop(ETH_LE  , $1, $3); }
  | expr '>'   expr { $$ = eth_ast_binop(ETH_GT  , $1, $3); }
  | expr GE    expr { $$ = eth_ast_binop(ETH_GE  , $1, $3); }
  | expr EQ    expr { $$ = eth_ast_binop(ETH_EQ  , $1, $3); }
  | expr NE    expr { $$ = eth_ast_binop(ETH_NE  , $1, $3); }
  | expr IS    expr { $$ = eth_ast_binop(ETH_IS  , $1, $3); }
  | expr ':'   expr { $$ = eth_ast_binop(ETH_CONS, $1, $3); }
  | expr '%'   expr { $$ = eth_ast_binop(ETH_MOD , $1, $3); }
  | expr '^'   expr { $$ = eth_ast_binop(ETH_POW , $1, $3); }
  | expr MOD   expr { $$ = eth_ast_binop(ETH_MOD , $1, $3); }
  | expr LAND  expr { $$ = eth_ast_binop(ETH_LAND, $1, $3); }
  | expr LOR   expr { $$ = eth_ast_binop(ETH_LOR , $1, $3); }
  | expr LXOR  expr { $$ = eth_ast_binop(ETH_LXOR, $1, $3); }
  | expr LSHL  expr { $$ = eth_ast_binop(ETH_LSHL, $1, $3); }
  | expr LSHR  expr { $$ = eth_ast_binop(ETH_LSHR, $1, $3); }
  | expr ASHL  expr { $$ = eth_ast_binop(ETH_ASHL, $1, $3); }
  | expr ASHR  expr { $$ = eth_ast_binop(ETH_ASHR, $1, $3); }
  | expr PPLUS expr {
    eth_ast *args[] = { $1, $3 };
    eth_ast *fn = eth_ast_cval(eth_get_builtin("++"));
    $$ = eth_ast_apply(fn, args, 2);
  }
  | '-' expr %prec UMINUS {
    $$ = eth_ast_binop(ETH_SUB, eth_ast_cval(eth_num(0)), $2);
  }
  | '+' expr %prec UPLUS {
    $$ = eth_ast_binop(ETH_ADD, eth_ast_cval(eth_num(0)), $2);
  }
  | NOT  expr { $$ = eth_ast_unop(ETH_NOT , $2); }
  | LNOT expr { $$ = eth_ast_unop(ETH_LNOT, $2); }
;

ident
  : SYMBOL
  | CAPSYMBOL '.' ident {
    int len1 = strlen($1);
    int len2 = strlen($3);
    $$ = malloc(len1 + 1 + len2 + 1);
    memcpy($$, $1, len1);
    $$[len1] = '.';
    memcpy($$ + len1 + 1, $3, len2);
    $$[len1 + 1 + len2] = 0;
    free($1);
    free($3);
  }
;

/*capident*/
  /*: CAPSYMBOL*/
  /*| CAPSYMBOL '.' capident {*/
    /*int len1 = strlen($1);*/
    /*int len2 = strlen($3);*/
    /*$$ = malloc(len1 + 1 + len2 + 1);*/
    /*memcpy($$, $1, len1);*/
    /*$$[len1] = '.';*/
    /*memcpy($$ + len1 + 1, $3, len2);*/
    /*$$[len1 + 1 + len2] = 0;*/
    /*free($1);*/
    /*free($3);*/
  /*}*/
/*;*/

args
  : atom {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | args atom {
    $$ = $1;
    cod_vec_push($$, $2);
  }
;

fnargs
  : { cod_vec_init($$); }
  | fnargs SYMBOL {
    $$ = $1;
    cod_vec_push($$, $2);
  }
;

binds
  : bind {
    cod_vec_init($$.pats);
    cod_vec_init($$.vals);
    cod_vec_push($$.pats, $1.pat);
    cod_vec_push($$.vals, $1.val);
  }
  | binds AND bind {
    $$ = $1;
    cod_vec_push($$.pats, $3.pat);
    cod_vec_push($$.vals, $3.val);
  }
;

bind
  : pattern '=' expr {
    $$.pat = $1;
    $$.val = $3;
  }
  | maybe_pub SYMBOL fnargs SYMBOL '=' expr {
    $$.pat = eth_ast_ident_pattern($2);
    $$.pat->ident.pub = $1;
    cod_vec_push($3, $4);
    $$.val = eth_ast_fn($3.data, $3.len, $6);
    free($2);
    cod_vec_iter($3, i, x, free(x));
    cod_vec_destroy($3);
  }
  | maybe_pub SYMBOL '!' '=' expr {
    $$.pat = eth_ast_ident_pattern($2);
    $$.pat->ident.pub = $1;
    $$.val = eth_ast_fn(NULL, 0, $5);
    free($2);
  }
;

maybe_pub: { $$ = false; } | PUB { $$ = true; };

string
  : '"' string_aux '"' {
    cod_vec_push($2, 0);
    $$ = $2.data;
  }
;

string_aux
  : { cod_vec_init($$); }
  | string_aux CHAR {
    $$ = $1;
    cod_vec_push($$, $2);
  }
;

atomic_pattern
  : maybe_pub SYMBOL {
    $$ = eth_ast_ident_pattern($2);
    $$->ident.pub = $1;
    free($2);
  }
  | CAPSYMBOL { $$ = eth_ast_symbol_pattern(eth_sym($1)); free($1); }
  | '(' pattern ')' { $$ = $2; }
;

pattern
  : atomic_pattern
  | pattern ':' pattern {
    char *fields[2] = { "car", "cdr" };
    eth_ast_pattern *pats[2] = { $1, $3 };
    $$ = eth_ast_unpack_pattern("pair", fields, pats, 2);
  }
;

maybe_imports
  : { $$.data = NULL; }
  | '(' import_list ')' { $$ = $2; }
;

import_list
  : ident {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | import_list ',' ident {
    $$ = $1;
    cod_vec_push($$, $3);
  }
;

list
  : expr {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | list ',' expr {
    $$ = $1;
    cod_vec_push($$, $3);
  }
;

%%

static const char*
filename(FILE *fp)
{
  char proclnk[0xFFF];
  static char filename[PATH_MAX];
  int fno;
  ssize_t r;

  fno = fileno(fp);
  sprintf(proclnk, "/proc/self/fd/%d", fno);
  r = readlink(proclnk, filename, PATH_MAX);
  if (r < 0)
    return NULL;
  filename[r] = '\0';
  return filename;
}

eth_ast*
eth_parse(FILE *stream)
{
  eth_scanner *scan = eth_create_scanner(stream);

  g_result = NULL;
  g_iserror = false;
  yyparse(scan);

  eth_destroy_scanner(scan);
  if (g_iserror)
  {
    if (g_result)
      eth_drop_ast(g_result);
    g_result = NULL;
  }
  return g_result;
}

void
yyerror(void *locp_ptr, eth_scanner *yyscanner, const char *what)
{
  YYLTYPE *locp = locp_ptr;

  g_iserror = true;
  eth_warning("%s", what);
  const char *path = filename(eth_get_scanner_input(yyscanner));
  if (path)
  {
    eth_warning("%s:%d:%d", path, locp->first_line, locp->first_column);
    /*opi_assert(opi_show_location(eth_error, path, locp->first_column,*/
        /*locp->first_line, locp->last_column, locp->last_line) == OPI_OK);*/
  }
}

