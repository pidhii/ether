%define parse.error verbose
/* Just for the sake of safety. */
%define parse.lac full
/* LAR(1) has issues after adding module-syntax. */
%define lr.type ielr
/* Reentrant parser: */
%define api.pure true
%param {eth_scanner *yyscanner}
/* Enable access to code locations. */
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

  typedef struct {
    cod_vec(eth_ast_pattern*) pats;
    cod_vec(eth_ast*) vals;
  } binds_t;

  typedef struct {
    bool rec;
    binds_t binds;
  } module_elt;
}

%union {
  eth_ast *ast;
  eth_number_t number;
  eth_t constant;
  char *string;
  char character;
  bool boolean;
  int integer;
  binds_t binds;
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
%token START_REPL UNDEFINED
%token START_FORMAT END_FORMAT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%token<number> NUMBER
%token<string> SYMBOL CAPSYMBOL
%token<constant> CONST
%token<character> CHAR
%token<string> STRING
%token HELP
%token START_REGEXP
%token<integer> END_REGEXP
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%nonassoc IN FN IFLET WHENLET
%nonassoc OPEN USING AS UNQUALIFIED MODULE
%nonassoc DOT_OPEN1 DOT_OPEN2
%nonassoc LARROW
%nonassoc PUB BUILTIN DEPRECATED
%nonassoc LIST_DDOT
%nonassoc BEGINN END
%nonassoc DO DONE
%nonassoc CASE OF
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%right RARROW
%right ';'
%nonassoc LET REC AND ASSERT
%left LAZY
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%right TERNARY
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
%nonassoc '<' LE '>' GE EQ NE IS ISNOT EQUAL NOTEQUAL DDOT DDDOT EQ_TILD
// level 4:
%right CONS PPLUS
// level 5:
%left '+' '-'
// level 6:
%left '*' '/' MOD LAND LOR LXOR
// level 7:
%right '^' LSHL LSHR ASHL ASHR
// level 8:
%left<string> USROP
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%left UMINUS UPLUS NOT LNOT
%right COMPOSE
%nonassoc '!'
%left '.' ':'


// =============================================================================
%type<ast> FnAtom Atom
%type<ast> Form
%type<ast> Expr
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%type<string> Ident
%type<string> CapIdent
%type<binds> Binds
%type<bind> Bind
%type<integer> Attribute
%type<astvec> Args
%type<patvec> FnArgs
%type<string> Help MaybeHelp
%type<charvec> String
%type<ast> FmtString FmtStringAux
%type<ast> RegExp
%type<charvec> StringAux
%type<pattern> AtomicPattern FormPattern Pattern
%type<strvec> MaybeImports ImportList
%type<boolean> MaybeComa
%type<astvec> List
%type<patvec> PatternList
%type<boolean> MaybeComaDots
%type<record> Record
%type<record_pattern> ReocrdPattern
%type<lc_aux> LcAux
%type<ast> ModuleBody
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%start Entry


%%

Entry
  : Expr { g_result = $1; }
  | START_REPL LET Binds {
    g_result = eth_ast_let($3.pats.data, $3.vals.data, $3.pats.len, eth_ast_cval(eth_nil));
    cod_vec_destroy($3.pats);
    cod_vec_destroy($3.vals);
  }
  | START_REPL LET REC Binds {
    g_result = eth_ast_letrec($4.pats.data, $4.vals.data, $4.pats.len, eth_ast_cval(eth_nil));
    cod_vec_destroy($4.pats);
    cod_vec_destroy($4.vals);
  }
  | START_REPL Expr {
    g_result = $2;
  }
;

FnAtom
  : Ident { $$ = eth_ast_ident($1); free($1); LOC($$, @$); }
  | '(' Expr ')' { $$ = $2; }
  | BEGINN Expr END { $$ = $2; }
  | FnAtom '!' { $$ = eth_ast_apply($1, NULL, 0); LOC($$, @$); }
  | CapIdent DOT_OPEN1 Expr ')' {
    $$ = eth_ast_import($1, "", NULL, 0, $3);
    LOC($$, @$);
    free($1);
  }
  | CapIdent DOT_OPEN1 List ',' Expr ')' {
    cod_vec_push($3, $5);
    int n = $3.len;
    char fieldsbuf[n][22];
    char *fields[n];
    for (int i = 0; i < n; ++i)
    {
      fields[i] = fieldsbuf[i];
      sprintf(fields[i], "_%d", i+1);
    }
    eth_ast *tuple = eth_ast_make_record(eth_tuple_type(n), fields,
        $3.data, n);
    cod_vec_destroy($3);
    // --
    $$ = eth_ast_import($1, "", NULL, 0, tuple);
    LOC($$, @$);
    free($1);
  }
  | CapIdent DOT_OPEN2 List MaybeComa ']' {
    eth_ast *acc = eth_ast_cval(eth_nil);
    cod_vec_riter($3, i, x, acc = eth_ast_binop(ETH_CONS, x, acc));
    cod_vec_destroy($3);
    LOC(acc, $3);

    $$ = eth_ast_import($1, "", NULL, 0, acc);
    LOC($$, @1);
    free($1);
  }
  | Atom '.' SYMBOL {
    $$ = eth_ast_access($1, $3);
    free($3);
  }
  | Atom ':' SYMBOL {
    eth_ast *access = eth_ast_access($1, $3);
    $$ = eth_ast_apply(access, &$1, 1);
    free($3);
  }
;

Atom
  : FnAtom
  | CAPSYMBOL { $$ = eth_ast_cval(eth_sym($1)); free($1); LOC($$, @$); }
  | NUMBER { $$ = eth_ast_cval(eth_create_number($1)); LOC($$, @$); }
  | CONST  { $$ = eth_ast_cval($1); LOC($$, @$); }
  | String {
    cod_vec_push($1, 0);
    $$ = eth_ast_cval(eth_create_string_from_ptr2($1.data, $1.len - 1));
    LOC($$, @$);
  }
  | FmtString
  | RegExp
  | '('')' { $$ = eth_ast_cval(eth_nil); LOC($$, @$); }
  | '['']' { $$ = eth_ast_cval(eth_nil); }
  | '[' List MaybeComa ']' {
    $$ = eth_ast_cval(eth_nil);
    cod_vec_riter($2, i, x, $$ = eth_ast_binop(ETH_CONS, x, $$));
    cod_vec_destroy($2);
    LOC($$, @$);
  }
  | '[' Expr DDOT Expr ']' %prec LIST_DDOT {
    eth_ast *p[2] = { $2, $4 };
    eth_ast *range = eth_ast_cval(eth_get_builtin("__inclusive_range"));
    $$ = eth_ast_apply(range, p, 2);
  }
  | '[' Expr '|' LcAux ']' {
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
      eth_t mapfn = eth_get_builtin("__List_filter_map");
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
      eth_t mapfn = eth_get_builtin("__List_map");
      assert(mapfn);
      eth_ast *map = eth_ast_cval(mapfn);
      // ---
      eth_ast *p[] = { fn, $4.in.expr };
      $$ = eth_ast_apply(map, p, 2);
    }
  }
  | '(' List ',' Expr ')' {
    cod_vec_push($2, $4);
    int n = $2.len;
    char fieldsbuf[n][22];
    char *fields[n];
    for (int i = 0; i < n; ++i)
    {
      fields[i] = fieldsbuf[i];
      sprintf(fields[i], "_%d", i+1);
    }
    $$ = eth_ast_make_record(eth_tuple_type(n), fields, $2.data, n);
    cod_vec_destroy($2);
  }
  | '{' Record MaybeComa '}' {
    eth_type *type = eth_record_type($2.keys.data, $2.keys.len);
    $$ = eth_ast_make_record(type, $2.keys.data, $2.vals.data, $2.keys.len);
    cod_vec_iter($2.keys, i, x, free(x));
    cod_vec_destroy($2.keys);
    cod_vec_destroy($2.vals);
  }
  | '{' Expr WITH Record MaybeComa '}' {
    $$ = eth_ast_update($2, $4.vals.data, $4.keys.data, $4.vals.len);
    cod_vec_destroy($4.vals);
    cod_vec_iter($4.keys, i, x, free(x));
    cod_vec_destroy($4.keys);
  }
  | MODULE CAPSYMBOL '=' ModuleBody END {
    $$ = eth_ast_module($2, $4);
    LOC($$, @1);
    free($2);
  }
  | MODULE '=' ModuleBody END { $$ = $3; }
;

Form
  : Atom
  | FnAtom Args {
    $$ = eth_ast_apply($1, $2.data, $2.len);
    LOC($$, @$);
    cod_vec_destroy($2);
  }
  | CAPSYMBOL Atom {
    eth_type *type = eth_variant_type($1);
    char *_0 = "_0";
    $$ = eth_ast_make_record(type, &_0, &$2, 1);
    free($1);
    LOC($$, @$);
  }
;

Expr
  : Form
  | OPEN CapIdent MaybeImports IN Expr {
    if ($3.data)
      $$ = eth_ast_import($2, NULL, $3.data, $3.len, $5);
    else
      $$ = eth_ast_import($2, "", NULL, 0, $5);
    free($2);
    if ($3.data)
    {
      cod_vec_iter($3, i, x, free(x));
      cod_vec_destroy($3);
    }
    LOC($$, @1);
  }
  | USING CapIdent AS CAPSYMBOL IN Expr {
    $$ = eth_ast_import($2, $4, NULL, 0, $6);
    free($2);
    free($4);
    LOC($$, @1);
  }
  | TRY Expr WITH Pattern RARROW Expr {
    $$ = eth_ast_try($4, $2, $6, 1);
    LOC($$, @$);
  }
  | Expr OR Expr { $$ = eth_ast_try(NULL, $1, $3, 0); LOC($$, @$); }
  | LET Binds IN Expr {
    $$ = eth_ast_let($2.pats.data, $2.vals.data, $2.pats.len, $4);
    cod_vec_destroy($2.pats);
    cod_vec_destroy($2.vals);
    LOC($$, @$);
  }
  | LET REC Binds IN Expr {
    $$ = eth_ast_letrec($3.pats.data, $3.vals.data, $3.pats.len, $5);
    cod_vec_destroy($3.pats);
    cod_vec_destroy($3.vals);
    LOC($$, @$);
  }
  | IFLET Pattern '=' Expr THEN Expr ELSE Expr {
    $$ = eth_ast_match($2, $4, $6, $8);
    LOC($$, @$);
  }
  | WHENLET Pattern '=' Expr THEN Expr {
    $$ = eth_ast_match($2, $4, $6, eth_ast_cval(eth_nil));
    LOC($$, @$);
  }
  | Expr IS OF Pattern {
    $$ = eth_ast_match($4, $1, eth_ast_cval(eth_true), eth_ast_cval(eth_false));
    LOC($$, @$);
  }
  | IF Expr THEN Expr ELSE Expr {
    $$ = eth_ast_if($2, $4, $6);
    LOC($$, @$);
  }
  | Expr IF Expr ELSE Expr %prec TERNARY {
    $$ = eth_ast_if($3, $1, $5);
    LOC($$, @$);
  }
  | WHEN Expr THEN Expr {
    $$ = eth_ast_if($2, $4, eth_ast_cval(eth_nil));
    LOC($$, @$);
  }
  | Expr WHEN Expr %prec TERNARY {
    $$ = eth_ast_if($3, $1, eth_ast_cval(eth_nil));
    LOC($$, @$);
  }
  | UNLESS Expr THEN Expr {
    $$ = eth_ast_if($2, eth_ast_cval(eth_nil), $4);
    LOC($$, @$);
  }
  | Expr UNLESS Expr %prec TERNARY {
    $$ = eth_ast_if($3, eth_ast_cval(eth_nil), $1);
    LOC($$, @$);
  }
  | ASSERT Expr IN Expr { $$ = eth_ast_assert($2, $4); }
  | FN FnArgs RARROW Expr {
    $$ = eth_ast_fn_with_patterns($2.data, $2.len, $4);
    cod_vec_destroy($2);
    LOC($$, @$);
  }
  | SYMBOL RARROW Expr {
    $$ = eth_ast_fn(&$1, 1, $3);
    free($1);
    LOC($$, @$);
  }
  | Expr '$' Expr {
    if ($1->tag == ETH_AST_APPLY)
    {
      $$ = $1;
      eth_ast_append_arg($$, $3);
    }
    else
      $$ = eth_ast_apply($1, &$3, 1);
    LOC($$, @$);
  }
  | Expr PIPE Expr {
    if ($3->tag == ETH_AST_APPLY)
    {
      $$ = $3;
      eth_ast_append_arg($$, $1);
    }
    else
      $$ = eth_ast_apply($3, &$1, 1);
    LOC($$, @$);
  }
  | Expr OPAND Expr { $$ = eth_ast_and($1, $3); LOC($$, @$); }
  | Expr OPOR  Expr { $$ = eth_ast_or($1, $3); LOC($$, @$); }
  | Expr ';'   Expr { $$ = eth_ast_seq($1, $3); LOC($$, @$); }
  | Expr ';'        { $$ = $1; LOC($$, @$); }
  | Expr '+'   Expr { $$ = eth_ast_binop(ETH_ADD , $1, $3); LOC($$, @$); }
  | Expr '-'   Expr { $$ = eth_ast_binop(ETH_SUB , $1, $3); LOC($$, @$); }
  | Expr '*'   Expr { $$ = eth_ast_binop(ETH_MUL , $1, $3); LOC($$, @$); }
  | Expr '/'   Expr { $$ = eth_ast_binop(ETH_DIV , $1, $3); LOC($$, @$); }
  | Expr '<'   Expr { $$ = eth_ast_binop(ETH_LT  , $1, $3); LOC($$, @$); }
  | Expr LE    Expr { $$ = eth_ast_binop(ETH_LE  , $1, $3); LOC($$, @$); }
  | Expr '>'   Expr { $$ = eth_ast_binop(ETH_GT  , $1, $3); LOC($$, @$); }
  | Expr GE    Expr { $$ = eth_ast_binop(ETH_GE  , $1, $3); LOC($$, @$); }
  | Expr EQ    Expr { $$ = eth_ast_binop(ETH_EQ  , $1, $3); LOC($$, @$); }
  | Expr NE    Expr { $$ = eth_ast_binop(ETH_NE  , $1, $3); LOC($$, @$); }
  | Expr IS    Expr { $$ = eth_ast_binop(ETH_IS  , $1, $3); LOC($$, @$); }
  | Expr EQUAL Expr { $$ = eth_ast_binop(ETH_EQUAL,$1, $3); LOC($$, @$); }
  | Expr CONS  Expr { $$ = eth_ast_binop(ETH_CONS, $1, $3); LOC($$, @$); }
  | Expr MOD   Expr { $$ = eth_ast_binop(ETH_MOD , $1, $3); LOC($$, @$); }
  | Expr '^'   Expr { $$ = eth_ast_binop(ETH_POW , $1, $3); LOC($$, @$); }
  | Expr LAND  Expr { $$ = eth_ast_binop(ETH_LAND, $1, $3); LOC($$, @$); }
  | Expr LOR   Expr { $$ = eth_ast_binop(ETH_LOR , $1, $3); LOC($$, @$); }
  | Expr LXOR  Expr { $$ = eth_ast_binop(ETH_LXOR, $1, $3); LOC($$, @$); }
  | Expr LSHL  Expr { $$ = eth_ast_binop(ETH_LSHL, $1, $3); LOC($$, @$); }
  | Expr LSHR  Expr { $$ = eth_ast_binop(ETH_LSHR, $1, $3); LOC($$, @$); }
  | Expr ASHL  Expr { $$ = eth_ast_binop(ETH_ASHL, $1, $3); LOC($$, @$); }
  | Expr ASHR  Expr { $$ = eth_ast_binop(ETH_ASHR, $1, $3); LOC($$, @$); }
  | Expr ISNOT Expr {
    $$ = eth_ast_unop(ETH_NOT, eth_ast_binop(ETH_IS, $1, $3));
    LOC($$, @$);
  }
  | Expr NOTEQUAL Expr {
    $$ = eth_ast_unop(ETH_NOT, eth_ast_binop(ETH_EQUAL, $1, $3));
    LOC($$, @$);
  }
  | Expr COMPOSE Expr {
    eth_ast *args[] = { $1, $3 };
    eth_ast *fn = eth_ast_ident("∘");
    $$ = eth_ast_apply(fn, args, 2);
    LOC($$, @$);
  }
  | Expr PPLUS Expr {
    eth_ast *args[] = { $1, $3 };
    eth_ast *fn = eth_ast_ident("++");
    $$ = eth_ast_apply(fn, args, 2);
    LOC($$, @$);
  }
  | Expr EQ_TILD Expr {
    eth_ast *args[] = { $1, $3 };
    eth_ast *fn = eth_ast_ident("=~");
    $$ = eth_ast_apply(fn, args, 2);
    LOC($$, @$);
  }
  | Expr DDOT Expr {
    char *fields[] = { "l", "r" };
    eth_ast *vals[] = { $1, $3 };
    $$ = eth_ast_make_record(eth_rangelr_type, fields, vals, 2);
  }
  | Expr DDDOT {
    char *fields[] = { "l" };
    eth_ast *vals[] = { $1 };
    $$ = eth_ast_make_record(eth_rangel_type, fields, vals, 1);
  }
  | DDDOT Expr {
    char *fields[] = { "r" };
    eth_ast *vals[] = { $2 };
    $$ = eth_ast_make_record(eth_ranger_type, fields, vals, 1);
  }
  | '-' Expr %prec UMINUS {
    $$ = eth_ast_binop(ETH_SUB, eth_ast_cval(eth_num(0)), $2);
    LOC($$, @$);
  }
  | '+' Expr %prec UPLUS {
    $$ = eth_ast_binop(ETH_ADD, eth_ast_cval(eth_num(0)), $2);
    LOC($$, @$);
  }
  | NOT  Expr { $$ = eth_ast_unop(ETH_NOT , $2); LOC($$, @$); }
  | LNOT Expr { $$ = eth_ast_unop(ETH_LNOT, $2); LOC($$, @$); }
  | Expr USROP Expr {
    eth_ast *args[] = { $1, $3 };
    eth_ast *fn = eth_ast_ident($2);
    $$ = eth_ast_apply(fn, args, 2);
    LOC($$, @$);
    free($2);
  }
  | LAZY Expr {
    eth_t lazy = eth_get_builtin("__Lazy_create");
    assert(lazy);
    eth_ast *thunk = eth_ast_fn(NULL, 0, $2);
    $$ = eth_ast_apply(eth_ast_cval(lazy), &thunk, 1);
  }
;

Ident
  : SYMBOL
  | CAPSYMBOL '.' Ident {
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

CapIdent
  : CAPSYMBOL
  | CAPSYMBOL '.' CapIdent {
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

Args
  : Atom {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | Args Atom {
    $$ = $1;
    cod_vec_push($$, $2);
  }
;

FnArgs
  : { cod_vec_init($$); }
  | FnArgs AtomicPattern {
    $$ = $1;
    cod_vec_push($$, $2);
  }
;

Binds
  : Bind {
    cod_vec_init($$.pats);
    cod_vec_init($$.vals);
    cod_vec_push($$.pats, $1.pat);
    cod_vec_push($$.vals, $1.val);
  }
  | Binds AND Bind {
    $$ = $1;
    cod_vec_push($$.pats, $3.pat);
    cod_vec_push($$.vals, $3.val);
  }
;

Bind
  : Pattern '=' MaybeHelp Expr {
    $$.pat = $1;
    $$.val = $4;
    if ($3)
    {
      if ($1->tag == ETH_PATTERN_IDENT)
      {
        eth_set_help($1->ident.attr, $3);
        free($3);
      }
      else
      {
        eth_warning(".help will be ignored");
        if (g_filename)
        {
          eth_location *loc = location(&@$);
          eth_print_location(loc, stderr);
          eth_drop_location(loc);
        }
        free($3);
      }
    }
  }
  | Attribute SYMBOL FnArgs AtomicPattern '=' MaybeHelp Expr {
    $$.pat = eth_ast_ident_pattern($2);
    eth_attr *attr = eth_create_attr($1);
    if (g_filename)
      eth_set_location(attr, location(&@$));
    if ($6)
    {
      eth_set_help(attr, $6);
      free($6);
    }
    eth_ref_attr($$.pat->ident.attr = attr);
    cod_vec_push($3, $4);
    $$.val = eth_ast_fn_with_patterns($3.data, $3.len, $7);
    free($2);
    cod_vec_destroy($3);
  }
  | Attribute SYMBOL '!' '=' MaybeHelp Expr {
    $$.pat = eth_ast_ident_pattern($2);
    eth_attr *attr = eth_create_attr($1);
    if (g_filename)
      eth_set_location(attr, location(&@$));
    if ($5)
    {
      eth_set_help(attr, $5);
      free($5);
    }
    eth_ref_attr($$.pat->ident.attr = attr);
    $$.val = eth_ast_fn(NULL, 0, $6);
    free($2);
  }
;

Attribute
  : { $$ = 0; }
  | Attribute PUB        { $$ = $1 | ETH_ATTR_PUB;        }
  | Attribute BUILTIN    { $$ = $1 | ETH_ATTR_BUILTIN;    }
  | Attribute DEPRECATED { $$ = $1 | ETH_ATTR_DEPRECATED; }
;

String
  : '"' StringAux '"' { $$ = $2; }
;

FmtString
  : '"' FmtStringAux '"' {
    eth_ast *fmt = eth_ast_cval(eth_get_builtin("__fmt"));
    eth_ast *p[1] = { $2 };
    $$ = eth_ast_apply(fmt, p, 1);
  }
;

MaybeHelp
  : { $$ = NULL; }
  | Help

Help
  : HELP StringAux HELP {
    cod_vec_push($2, 0);
    $$ = $2.data;
    $2.data = NULL;
  }
;

RegExp
  : START_REGEXP StringAux END_REGEXP {
    cod_vec_push($2, 0);
    eth_t regexp = eth_create_regexp($2.data, $3, NULL, NULL);
    if (regexp == NULL)
    {
      eth_error("invalid regular expression");
      eth_location *loc = location(&@$);
      eth_print_location(loc, stderr);
      eth_drop_location(loc);
      abort();
    }
    eth_study_regexp(regexp);
    $$ = eth_ast_cval(regexp);
    free($2.data);
    LOC($$, @$);
  }
;

StringAux
  : { cod_vec_init($$); }
  | StringAux CHAR {
    $$ = $1;
    cod_vec_push($$, $2);
  }
  | StringAux STRING {
    $$ = $1;
    for (const char *p = $2; *p; ++p)
      cod_vec_push($$, *p);
    free($2);
  }
;

FmtStringAux
  /* To distinguish FmtString from simple String */
  : StringAux START_FORMAT Expr END_FORMAT StringAux {
    eth_ast *tmp;

    cod_vec_push($1, 0);
    tmp = eth_ast_cval(eth_create_string_from_ptr2($1.data, $1.len - 1));
    $$ = eth_ast_binop(ETH_CONS, tmp, eth_ast_cval(eth_nil));

    $$ = eth_ast_binop(ETH_CONS, $3, $$);
    /*LOC($$, @$);*/

    cod_vec_push($5, 0);
    tmp = eth_ast_cval(eth_create_string_from_ptr2($5.data, $5.len - 1));
    $$ = eth_ast_binop(ETH_CONS, tmp, $$);
  }
  | FmtStringAux START_FORMAT Expr END_FORMAT StringAux {
    eth_ast *tmp;

    $$ = $1;

    $$ = eth_ast_binop(ETH_CONS, $3, $$);
    /*LOC($$, @$);*/

    cod_vec_push($5, 0);
    tmp = eth_ast_cval(eth_create_string_from_ptr2($5.data, $5.len - 1));
    $$ = eth_ast_binop(ETH_CONS, tmp, $$);
  }
;

AtomicPattern
  : '_' { $$ = eth_ast_dummy_pattern(); }
  | Attribute SYMBOL {
    $$ = eth_ast_ident_pattern($2);
    eth_attr *attr = eth_create_attr($1);
    if (g_filename)
      eth_set_location(attr, location(&@$));
    eth_ref_attr($$->ident.attr = attr);
    free($2);
  }
  | CAPSYMBOL { $$ = eth_ast_constant_pattern(eth_sym($1)); free($1); }
  | CONST { $$ = eth_ast_constant_pattern($1); }
  | String {
    cod_vec_push($1, 0);
    eth_t str = eth_create_string_from_ptr2($1.data, $1.len - 1);
    $$ = eth_ast_constant_pattern(str);
  }
  | NUMBER { $$ = eth_ast_constant_pattern(eth_create_number($1)); }
  | '('')' { $$ = eth_ast_constant_pattern(eth_nil); }
  | '['']' { $$ = eth_ast_constant_pattern(eth_nil); }
  | '(' Pattern ')' { $$ = $2; }
  | '(' PatternList ',' Pattern ')' {
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
    $$ = eth_ast_unpack_pattern(type, fields, $2.data, n);
    // ---
    cod_vec_destroy($2);
  }
  | '{' ReocrdPattern '}' {
    $$ = eth_ast_record_pattern($2.keys.data, $2.vals.data, $2.keys.len);
    cod_vec_iter($2.keys, i, x, free(x));
    cod_vec_destroy($2.keys);
    cod_vec_destroy($2.vals);
  }
  | '[' PatternList MaybeComaDots ']' {
    int n = $2.len;
    // ---
    if ($3) $$ = eth_ast_dummy_pattern();
    else $$ = eth_ast_constant_pattern(eth_nil);
    // ---
    for (int i = n - 1; i >= 0; --i)
    {
      char *fields[2] = { "car", "cdr" };
      eth_ast_pattern *pats[2] = { $2.data[i], $$ };
      $$ = eth_ast_unpack_pattern(eth_pair_type, fields, pats, 2);
    }
    // ---
    cod_vec_destroy($2);
  }
;

MaybeComaDots
  : { $$ = false; }
  | ',' DDDOT { $$ = true; }
;

FormPattern
  : AtomicPattern
  | CAPSYMBOL AtomicPattern {
    eth_type *type = eth_variant_type($1);
    char *_0 = "_0";
    $$ = eth_ast_unpack_pattern(type, &_0, &$2, 1);
    free($1);
  }
;

Pattern
  : FormPattern
  | Pattern CONS Pattern {
    char *fields[2] = { "car", "cdr" };
    eth_ast_pattern *pats[2] = { $1, $3 };
    $$ = eth_ast_unpack_pattern(eth_pair_type, fields, pats, 2);
  }
  | Pattern DDOT Pattern {
    char *fields[2] = { "l", "r" };
    eth_ast_pattern *pats[2] = { $1, $3 };
    $$ = eth_ast_unpack_pattern(eth_rangelr_type, fields, pats, 2);
  }
  | Pattern DDDOT {
    char *fields[1] = { "l" };
    eth_ast_pattern *pats[1] = { $1 };
    $$ = eth_ast_unpack_pattern(eth_rangel_type, fields, pats, 1);
  }
  | DDDOT Pattern {
    char *fields[1] = { "r" };
    eth_ast_pattern *pats[1] = { $2 };
    $$ = eth_ast_unpack_pattern(eth_ranger_type, fields, pats, 1);
  }
  | Pattern AS SYMBOL {
    $$ = $1;
    eth_set_pattern_alias($$, $3);
    free($3);
  }
;

PatternList
  : Pattern {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | PatternList ',' Pattern {
    $$ = $1;
    cod_vec_push($$, $3);
  }
;

ReocrdPattern
  : SYMBOL {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, eth_ast_ident_pattern($1));
  }
  | SYMBOL '=' Pattern {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, $3);
  }
  | ReocrdPattern ',' SYMBOL {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, eth_ast_ident_pattern($3));
  }
  | ReocrdPattern ',' SYMBOL '=' Pattern {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, $5);
  }
;

MaybeImports
  : { $$.data = NULL; }
  | '(' ImportList ')' { $$ = $2; }
;

ImportList
  : Ident {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | ImportList ',' Ident {
    $$ = $1;
    cod_vec_push($$, $3);
  }
;

List
  : Expr {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | List ',' Expr {
    $$ = $1;
    cod_vec_push($$, $3);
  }
;

Record
  : SYMBOL '=' Expr {
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
  | Record ',' SYMBOL '=' Expr {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, $5);
  }
  | Record ',' SYMBOL {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, eth_ast_ident($3));
  }
;

LcAux
  : Pattern LARROW Expr {
    $$.in.pat = $1;
    $$.in.expr = $3;
    $$.pred = NULL;
  }
  | Pattern LARROW Expr ',' Expr {
    $$.in.pat = $1;
    $$.in.expr = $3;
    $$.pred = $5;
  }
;

MaybeComa
  : { $$ = false; }
  | ',' { $$ = true; }
;

ModuleBody
  : { $$ = eth_ast_cval(eth_nil); }
  | LET Binds ModuleBody {
    $$ = eth_ast_let($2.pats.data, $2.vals.data, $2.pats.len, $3);
    cod_vec_destroy($2.pats);
    cod_vec_destroy($2.vals);
    LOC($$, @$);
  }
  | LET REC Binds ModuleBody {
    $$ = eth_ast_letrec($3.pats.data, $3.vals.data, $3.pats.len, $4);
    cod_vec_destroy($3.pats);
    cod_vec_destroy($3.vals);
    LOC($$, @$);
  }
  | OPEN CapIdent MaybeImports ModuleBody {
    if ($3.data)
      $$ = eth_ast_import($2, NULL, $3.data, $3.len, $4);
    else
      $$ = eth_ast_import($2, "", NULL, 0, $4);
    free($2);
    if ($3.data)
    {
      cod_vec_iter($3, i, x, free(x));
      cod_vec_destroy($3);
    }
    LOC($$, @1);
  }
  | USING CapIdent AS CAPSYMBOL ModuleBody {
    $$ = eth_ast_import($2, $4, NULL, 0, $5);
    free($2);
    free($4);
    LOC($$, @1);
  }
  | MODULE CAPSYMBOL '=' ModuleBody END ModuleBody {
    eth_ast *mod = eth_ast_module($2, $4);
    LOC(mod, @1);
    free($2);
    $$ = eth_ast_seq(mod, $6);
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

eth_ast*
eth_parse_repl(FILE *stream)
{
  _eth_start_token = START_REPL;
  return eth_parse(stream);
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

