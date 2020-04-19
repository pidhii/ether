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

static eth_location*
location(void *locp_ptr);

#define LOC(ast, locp)                            \
  do {                                            \
    if (g_filename)                               \
      eth_set_ast_location(ast, location(&locp)); \
  } while (0)

static const char *g_filename = NULL;
static eth_ast* g_result;
static bool g_iserror;
int _eth_start_token = -1;
%}

%code requires {
  #include "codeine/vec.h"
  #include <assert.h>
  typedef struct {
    eth_ast_pattern *pat;
    eth_ast *expr;
  } lc_in;
}

%union {
  eth_ast *ast;
  eth_number_t number;
  eth_t constant;
  char *string;
  char character;
  bool boolean;
  int integer;
  struct {
    cod_vec(eth_ast_pattern*) pats;
    cod_vec(eth_ast*) vals;
  } binds;
  struct {
    eth_ast_pattern *pat;
    eth_ast *val;
  } bind;
  struct {
    cod_vec(char*) keys;
    cod_vec(eth_ast*) vals;
  } record;
  struct {
    cod_vec(char*) keys;
    cod_vec(eth_ast_pattern*) vals;
  } record_pattern;

  struct {
    lc_in in;
    eth_ast *pred;
  } lc_aux;

  cod_vec(eth_ast*) astvec;
  cod_vec(char*) strvec;
  cod_vec(char) charvec;
  eth_ast_pattern *pattern;
  cod_vec(eth_ast_pattern*) patvec;
}

%destructor {
  eth_drop_ast($$);
} <ast>

%destructor {
  eth_drop($$);
} <constant>

%destructor {
  if ($$)
    free($$);
} <string>

%destructor {
  eth_drop_ast_pattern($$.pat);
  eth_drop_ast($$.val);
} <bind>

%destructor {
  cod_vec_iter($$.pats, i, x, eth_drop_ast_pattern(x));
  cod_vec_iter($$.vals, i, x, eth_drop_ast(x));
  cod_vec_destroy($$.pats);
  cod_vec_destroy($$.vals);
} <binds>

%destructor {
  cod_vec_iter($$.keys, i, x, free(x));
  cod_vec_iter($$.vals, i, x, eth_drop_ast(x));
  cod_vec_destroy($$.keys);
  cod_vec_destroy($$.vals);
} <record>

%destructor {
  cod_vec_iter($$.keys, i, x, free(x));
  cod_vec_iter($$.vals, i, x, eth_drop_ast_pattern(x));
  cod_vec_destroy($$.keys);
  cod_vec_destroy($$.vals);
} <record_pattern>

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
  eth_drop_ast_pattern($$);
} <pattern>

%destructor {
  cod_vec_iter($$, i, x, eth_drop_ast_pattern(x));
  cod_vec_destroy($$);
} <patvec>

%destructor {
  eth_drop_ast_pattern($$.in.pat);
  eth_drop_ast($$.in.expr);
  if ($$.pred)
    eth_drop_ast($$.pred);
} <lc_aux>

// =============================================================================
%token UNDEFINED
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%token<number> NUMBER
%token<string> SYMBOL CAPSYMBOL
%token<constant> CONST
%token<character> CHAR
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%nonassoc LET REC AND IN FN IFLET WHENLET
%nonassoc IMPORT AS UNQUALIFIED
%nonassoc DOT_OPEN1 DOT_OPEN2
%nonassoc LARROW
%nonassoc PUB BUILTIN
%nonassoc LIST_DDOT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%right RARROW
%right ';'
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%nonassoc IF THEN ELSE WHEN UNLESS TRY WITH
%right OR
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// level 0:
%left PIPE
%right '$'
// level 1:
%right OPOR
// level 2:
%right OPAND
// level 3:
%nonassoc '<' LE '>' GE EQ NE IS EQUAL DDOT DDDOT
// level 4:
%right CONS PPLUS
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
%type<ast> fn_atom atom
%type<ast> form
%type<ast> expr
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%type<string> ident
%type<string> capident
%type<binds> binds
%type<bind> bind
%type<integer> attribute
%type<astvec> args
%type<patvec> fnargs
%type<string> string
%type<charvec> string_aux
%type<pattern> atomic_pattern form_pattern pattern
%type<strvec> maybe_imports import_list
%type<astvec> list list_with_coma
%type<patvec> pattern_list
%type<record> record
%type<record_pattern> record_pattern
%type<lc_aux> lc_aux
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%start entry


%%

entry
  : expr { g_result = $1; }
;

fn_atom
  : ident { $$ = eth_ast_ident($1); free($1); LOC($$, @$); }
  | '(' expr ')' { $$ = $2; }
  | atom '!' { $$ = eth_ast_apply($1, NULL, 0); LOC($$, @$); }
  | capident DOT_OPEN1 expr ')' {
    $$ = eth_ast_import($1, "", NULL, 0, $3);
    LOC($$, @$);
    free($1);
  }
  | capident DOT_OPEN2 list_with_coma ']' {
    eth_ast *acc = eth_ast_cval(eth_nil);
    cod_vec_riter($3, i, x, acc = eth_ast_binop(ETH_CONS, x, acc));
    cod_vec_destroy($3);
    LOC(acc, $3);

    $$ = eth_ast_import($1, "", NULL, 0, acc);
    LOC($$, @$);
    free($1);
  }
;

atom
  : fn_atom
  | CAPSYMBOL { $$ = eth_ast_cval(eth_sym($1)); free($1); LOC($$, @$); }
  | NUMBER { $$ = eth_ast_cval(eth_create_number($1)); LOC($$, @$); }
  | CONST  { $$ = eth_ast_cval($1); LOC($$, @$); }
  | string { $$ = eth_ast_cval(eth_create_string($1)); free($1); LOC($$, @$); }
  | '('')' { $$ = eth_ast_cval(eth_nil); LOC($$, @$); }
  | '['']' { $$ = eth_ast_cval(eth_nil); }
  | '[' list_with_coma ']' {
    $$ = eth_ast_cval(eth_nil);
    cod_vec_riter($2, i, x, $$ = eth_ast_binop(ETH_CONS, x, $$));
    cod_vec_destroy($2);
    LOC($$, @$);
  }
  | '[' expr DDOT expr ']' %prec LIST_DDOT {
    eth_ast *p[2] = { $2, $4 };
    eth_ast *range = eth_ast_cval(eth_get_builtin("__inclusive_range"));
    $$ = eth_ast_apply(range, p, 2);
  }
  | '[' expr '|' lc_aux ']' {
    eth_ast_pattern *fnargs[] = { $4.in.pat };
    // ---
    eth_ast *fn;
    if ($4.pred)
    {
      eth_ast *filterout = eth_ast_cval(eth_sym("Filter_out"));
      eth_ast *raise = eth_ast_cval(eth_get_builtin("raise"));
      eth_ast *elsebr = eth_ast_apply(raise, &filterout, 1);
      eth_ast *body = eth_ast_if($4.pred, $2, elsebr);
      fn = eth_ast_fn_with_patterns(fnargs, 1, body);
      // ---
      eth_t mapfn = eth_get_builtin("filter_map");
      assert(mapfn);
      eth_ast *map = eth_ast_cval(mapfn);
      // ---
      eth_ast *p[] = { fn, $4.in.expr };
      $$ = eth_ast_apply(map, p, 2);
    }
    else
    {
      fn = eth_ast_fn_with_patterns(fnargs, 1, $2);
      // ---
      eth_t mapfn = eth_get_builtin("map");
      assert(mapfn);
      eth_ast *map = eth_ast_cval(mapfn);
      // ---
      eth_ast *p[] = { fn, $4.in.expr };
      $$ = eth_ast_apply(map, p, 2);
    }
  }
  | '(' list ',' expr ')' {
    cod_vec_push($2, $4);
    int n = $2.len;
    char fieldsbuf[n][22];
    char *fields[n];
    for (int i = 0; i < n; ++i)
    {
      fields[i] = fieldsbuf[i];
      sprintf(fields[i], "_%d", i+1);
    }
    $$ = eth_ast_make_record_with_type(eth_tuple_type(n), fields, $2.data, n);
    cod_vec_destroy($2);
  }
  | '{' record '}' {
    eth_type *type = eth_record_type($2.keys.data, $2.keys.len);
    $$ = eth_ast_make_record_with_type(type, $2.keys.data, $2.vals.data, $2.keys.len);
    cod_vec_iter($2.keys, i, x, free(x));
    cod_vec_destroy($2.keys);
    cod_vec_destroy($2.vals);
  }
;

form
  : atom
  | fn_atom args {
    $$ = eth_ast_apply($1, $2.data, $2.len);
    LOC($$, @$);
    cod_vec_destroy($2);
  }
  | CAPSYMBOL atom {
    eth_type *type = eth_variant_type($1);
    char *_0 = "_0";
    $$ = eth_ast_make_record_with_type(type, &_0, &$2, 1);
    free($1);
    LOC($$, @$);
  }
;

expr
  : form
  | IMPORT capident maybe_imports IN expr {
    $$ = eth_ast_import($2, NULL, $3.data, $3.len, $5);
    free($2);
    if ($3.data)
    {
      cod_vec_iter($3, i, x, free(x));
      cod_vec_destroy($3);
    }
    LOC($$, @$);
  }
  | IMPORT capident AS CAPSYMBOL IN expr {
    $$ = eth_ast_import($2, $4, NULL, 0, $6);
    free($2);
    free($4);
    LOC($$, @$);
  }
  | IMPORT UNQUALIFIED capident IN expr {
    $$ = eth_ast_import($3, "", NULL, 0, $5);
    free($3);
    LOC($$, @$);
  }
  | TRY expr WITH pattern RARROW expr %prec WITH {
    $$ = eth_ast_try($4, $2, $6, 1);
    LOC($$, @$);
  }
  | expr OR expr { $$ = eth_ast_try(NULL, $1, $3, 0); LOC($$, @$); }
  | LET binds IN expr {
    $$ = eth_ast_let($2.pats.data, $2.vals.data, $2.pats.len, $4);
    cod_vec_destroy($2.pats);
    cod_vec_destroy($2.vals);
    LOC($$, @$);
  }
  | LET REC binds IN expr {
    $$ = eth_ast_letrec($3.pats.data, $3.vals.data, $3.pats.len, $5);
    cod_vec_destroy($3.pats);
    cod_vec_destroy($3.vals);
    LOC($$, @$);
  }
  | IFLET pattern '=' expr THEN expr ELSE expr {
    $$ = eth_ast_match($2, $4, $6, $8);
    LOC($$, @$);
  }
  | WHENLET pattern '=' expr THEN expr {
    $$ = eth_ast_match($2, $4, $6, eth_ast_cval(eth_nil));
    LOC($$, @$);
  }
  | IF expr THEN expr ELSE expr {
    $$ = eth_ast_if($2, $4, $6);
    LOC($$, @$);
  }
  | expr IF expr ELSE expr {
    $$ = eth_ast_if($3, $1, $5);
    LOC($$, @$);
  }
  | WHEN expr THEN expr {
    $$ = eth_ast_if($2, $4, eth_ast_cval(eth_nil));
    LOC($$, @$);
  }
  | expr WHEN expr {
    $$ = eth_ast_if($3, $1, eth_ast_cval(eth_nil));
    LOC($$, @$);
  }
  | UNLESS expr THEN expr {
    $$ = eth_ast_if($2, eth_ast_cval(eth_nil), $4);
    LOC($$, @$);
  }
  | expr UNLESS expr {
    $$ = eth_ast_if($3, eth_ast_cval(eth_nil), $1);
    LOC($$, @$);
  }
  | FN fnargs RARROW expr {
    $$ = eth_ast_fn_with_patterns($2.data, $2.len, $4);
    cod_vec_destroy($2);
    LOC($$, @$);
  }
  | expr '$' expr {
    if ($1->tag == ETH_AST_APPLY)
    {
      $$ = $1;
      eth_ast_append_arg($$, $3);
    }
    else
      $$ = eth_ast_apply($1, &$3, 1);
    LOC($$, @$);
  }
  | expr PIPE expr {
    if ($3->tag == ETH_AST_APPLY)
    {
      $$ = $3;
      eth_ast_append_arg($$, $1);
    }
    else
      $$ = eth_ast_apply($3, &$1, 1);
    LOC($$, @$);
  }
  | expr OPAND expr { $$ = eth_ast_and($1, $3); LOC($$, @$); }
  | expr OPOR  expr { $$ = eth_ast_or($1, $3); LOC($$, @$); }
  | expr ';'   expr { $$ = eth_ast_seq($1, $3); LOC($$, @$); }
  | expr ';'        { $$ = $1; LOC($$, @$); }
  | expr '+'   expr { $$ = eth_ast_binop(ETH_ADD , $1, $3); LOC($$, @$); }
  | expr '-'   expr { $$ = eth_ast_binop(ETH_SUB , $1, $3); LOC($$, @$); }
  | expr '*'   expr { $$ = eth_ast_binop(ETH_MUL , $1, $3); LOC($$, @$); }
  | expr '/'   expr { $$ = eth_ast_binop(ETH_DIV , $1, $3); LOC($$, @$); }
  | expr '<'   expr { $$ = eth_ast_binop(ETH_LT  , $1, $3); LOC($$, @$); }
  | expr LE    expr { $$ = eth_ast_binop(ETH_LE  , $1, $3); LOC($$, @$); }
  | expr '>'   expr { $$ = eth_ast_binop(ETH_GT  , $1, $3); LOC($$, @$); }
  | expr GE    expr { $$ = eth_ast_binop(ETH_GE  , $1, $3); LOC($$, @$); }
  | expr EQ    expr { $$ = eth_ast_binop(ETH_EQ  , $1, $3); LOC($$, @$); }
  | expr NE    expr { $$ = eth_ast_binop(ETH_NE  , $1, $3); LOC($$, @$); }
  | expr IS    expr { $$ = eth_ast_binop(ETH_IS  , $1, $3); LOC($$, @$); }
  | expr EQUAL expr { $$ = eth_ast_binop(ETH_EQUAL,$1, $3); LOC($$, @$); }
  | expr CONS  expr { $$ = eth_ast_binop(ETH_CONS, $1, $3); LOC($$, @$); }
  | expr '%'   expr { $$ = eth_ast_binop(ETH_MOD , $1, $3); LOC($$, @$); }
  | expr MOD   expr { $$ = eth_ast_binop(ETH_MOD , $1, $3); LOC($$, @$); }
  | expr '^'   expr { $$ = eth_ast_binop(ETH_POW , $1, $3); LOC($$, @$); }
  | expr LAND  expr { $$ = eth_ast_binop(ETH_LAND, $1, $3); LOC($$, @$); }
  | expr LOR   expr { $$ = eth_ast_binop(ETH_LOR , $1, $3); LOC($$, @$); }
  | expr LXOR  expr { $$ = eth_ast_binop(ETH_LXOR, $1, $3); LOC($$, @$); }
  | expr LSHL  expr { $$ = eth_ast_binop(ETH_LSHL, $1, $3); LOC($$, @$); }
  | expr LSHR  expr { $$ = eth_ast_binop(ETH_LSHR, $1, $3); LOC($$, @$); }
  | expr ASHL  expr { $$ = eth_ast_binop(ETH_ASHL, $1, $3); LOC($$, @$); }
  | expr ASHR  expr { $$ = eth_ast_binop(ETH_ASHR, $1, $3); LOC($$, @$); }
  | expr PPLUS expr {
    eth_ast *args[] = { $1, $3 };
    eth_ast *fn = eth_ast_cval(eth_get_builtin("++"));
    $$ = eth_ast_apply(fn, args, 2);
    LOC($$, @$);
  }
  | expr DDOT expr {
    char *fields[] = { "l", "r" };
    eth_ast *vals[] = { $1, $3 };
    $$ = eth_ast_make_record_with_type(eth_rangelr_type, fields, vals, 2);
  }
  | expr DDDOT {
    char *fields[] = { "l" };
    eth_ast *vals[] = { $1 };
    $$ = eth_ast_make_record_with_type(eth_rangel_type, fields, vals, 1);
  }
  | DDDOT expr {
    char *fields[] = { "r" };
    eth_ast *vals[] = { $2 };
    $$ = eth_ast_make_record_with_type(eth_ranger_type, fields, vals, 1);
  }
  | '-' expr %prec UMINUS {
    $$ = eth_ast_binop(ETH_SUB, eth_ast_cval(eth_num(0)), $2);
    LOC($$, @$);
  }
  | '+' expr %prec UPLUS {
    $$ = eth_ast_binop(ETH_ADD, eth_ast_cval(eth_num(0)), $2);
    LOC($$, @$);
  }
  | NOT  expr { $$ = eth_ast_unop(ETH_NOT , $2); LOC($$, @$); }
  | LNOT expr { $$ = eth_ast_unop(ETH_LNOT, $2); LOC($$, @$); }
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

capident
  : CAPSYMBOL
  | CAPSYMBOL '.' capident {
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

list_with_coma: list | list ',';

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
  | fnargs atomic_pattern {
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
  | attribute SYMBOL fnargs atomic_pattern '=' expr {
    $$.pat = eth_ast_ident_pattern($2);
    $$.pat->ident.attr = $1;
    cod_vec_push($3, $4);
    $$.val = eth_ast_fn_with_patterns($3.data, $3.len, $6);
    free($2);
    cod_vec_destroy($3);
  }
  | attribute SYMBOL '!' '=' expr {
    $$.pat = eth_ast_ident_pattern($2);
    $$.pat->ident.attr = $1;
    $$.val = eth_ast_fn(NULL, 0, $5);
    free($2);
  }
;

attribute
  : { $$ = 0; }
  | attribute PUB     { $$ = $1 | ETH_ATTR_PUB;     }
  | attribute BUILTIN { $$ = $1 | ETH_ATTR_BUILTIN; }
;

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
  : '_' { $$ = eth_ast_dummy_pattern(); }
  | attribute SYMBOL {
    $$ = eth_ast_ident_pattern($2);
    $$->ident.attr = $1;
    free($2);
  }
  | CAPSYMBOL { $$ = eth_ast_constant_pattern(eth_sym($1)); free($1); }
  | CONST { $$ = eth_ast_constant_pattern($1); }
  | '('')' { $$ = eth_ast_constant_pattern(eth_nil); }
  | '['']' { $$ = eth_ast_constant_pattern(eth_nil); }
  | '(' pattern ')' { $$ = $2; }
  | '(' pattern_list ',' pattern ')' {
    cod_vec_push($2, $4);
    int n = $2.len;
    // ---
    char fldbufs[n][21];
    char *fields[n];
    for (int i = 0; i < n; ++i)
    {
      fields[i] = fldbufs[i];
      sprintf(fields[i], "_%d", i+1);
    }
    // ---
    eth_type *type = eth_tuple_type(n);
    $$ = eth_ast_unpack_pattern_with_type(type, fields, $2.data, n);
    // ---
    cod_vec_destroy($2);
  }
  | '{' record_pattern '}' {
    $$ = eth_ast_record_pattern($2.keys.data, $2.vals.data, $2.keys.len);
    cod_vec_iter($2.keys, i, x, free(x));
    cod_vec_destroy($2.keys);
    cod_vec_destroy($2.vals);
  }
;

form_pattern
  : atomic_pattern
  | CAPSYMBOL atomic_pattern {
    eth_type *type = eth_variant_type($1);
    char *_0 = "_0";
    $$ = eth_ast_unpack_pattern_with_type(type, &_0, &$2, 1);
    free($1);
  }
;

pattern
  : form_pattern
  | pattern CONS pattern {
    char *fields[2] = { "car", "cdr" };
    eth_ast_pattern *pats[2] = { $1, $3 };
    $$ = eth_ast_unpack_pattern_with_type(eth_pair_type, fields, pats, 2);
  }
  | pattern DDOT pattern {
    char *fields[2] = { "l", "r" };
    eth_ast_pattern *pats[2] = { $1, $3 };
    $$ = eth_ast_unpack_pattern_with_type(eth_rangelr_type, fields, pats, 2);
  }
  | pattern DDDOT {
    char *fields[1] = { "l" };
    eth_ast_pattern *pats[1] = { $1 };
    $$ = eth_ast_unpack_pattern_with_type(eth_rangel_type, fields, pats, 1);
  }
  | DDDOT pattern {
    char *fields[1] = { "r" };
    eth_ast_pattern *pats[1] = { $2 };
    $$ = eth_ast_unpack_pattern_with_type(eth_ranger_type, fields, pats, 1);
  }
  | pattern AS SYMBOL {
    $$ = $1;
    eth_set_pattern_alias($$, $3);
    free($3);
  }
;

pattern_list
  : pattern {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | pattern_list ',' pattern {
    $$ = $1;
    cod_vec_push($$, $3);
  }
;

record_pattern
  : SYMBOL {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, eth_ast_ident_pattern($1));
  }
  | SYMBOL '=' SYMBOL {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, eth_ast_ident_pattern($3));
    free($3);
  }
  | record_pattern ',' SYMBOL {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, eth_ast_ident_pattern($3));
  }
  | record_pattern ',' SYMBOL '=' SYMBOL {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, eth_ast_ident_pattern($5));
    free($5);
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

record
  : SYMBOL '=' expr {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, $3);
  }
  | SYMBOL {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, eth_ast_ident($1));
  }
  | record ',' SYMBOL '=' expr {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, $5);
  }
  | record ',' SYMBOL {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, eth_ast_ident($3));
  }
;

lc_aux
  : pattern LARROW expr {
    $$.in.pat = $1;
    $$.in.expr = $3;
    $$.pred = NULL;
  }
  | pattern LARROW expr ',' expr {
    $$.in.pat = $1;
    $$.in.expr = $3;
    $$.pred = $5;
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
  g_filename = filename(stream);
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
    eth_location *loc = eth_create_location(locp->first_line, locp->first_column,
        locp->last_line, locp->last_column, path);
    eth_print_location(loc, stderr);
    eth_drop_location(loc);
  }
}

static eth_location*
location(void *locp_ptr)
{
  YYLTYPE *locp = locp_ptr;
  if (g_filename)
  {
    return eth_create_location(
        locp->first_line, locp->first_column,
        locp->last_line, locp->last_column,
        g_filename);
  }
  else
  {
    return NULL;
  }
}

