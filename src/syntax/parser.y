%define parse.error verbose
/* Just for the sake of safety. */
/*%define parse.lac full*/
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
#include "codeine/vec.h"

#include "scanner-data.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>


ETH_MODULE("ether:parser")


static const char *g_filename = NULL;
static eth_ast* g_result;
static bool g_iserror;


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

#define MESSAGE(type, l, fmt, ...) \
  do { \
    eth_##type(fmt, ##__VA_ARGS__); \
    eth_location *loc = location(&(l)); \
    eth_print_location_opt(loc, stderr, ETH_LOPT_FILE); \
    eth_drop_location(loc); \
  } while (0)


static eth_ast*
dummy_ast()
{ return eth_ast_cval(eth_nil); }

static bool
is_dummy(const eth_ast *ast)
{ return ast->tag == ETH_AST_CVAL && ast->cval.val == eth_nil; }

static eth_attr*
_create_attr(int aflag, void *locpp)
{
  eth_attr *attr = eth_create_attr(aflag);
  if (g_filename)
    eth_set_location(attr, location(locpp));
  return attr;
}
#define create_attr(aflag, locp) \
  _create_attr(aflag, &locp)

#define LOC(ast, locp)                            \
  do {                                            \
    if (g_filename)                               \
      eth_set_ast_location(ast, location(&locp)); \
  } while (0)

#define SCANROOT (eth_get_scanner_data(yyscanner)->root)
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

  cod_vec(eth_ast*) astvec;
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
  cod_vec_destroy($$);
} <charvec>

%destructor {
  eth_drop_ast_pattern($$);
} <pattern>

%destructor {
  cod_vec_iter($$, i, x, eth_drop_ast_pattern(x));
  cod_vec_destroy($$);
} <patvec>

// =============================================================================
%token START_REPL UNDEFINED
%token START_FORMAT END_FORMAT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%token<number> NUMBER
%token<string> SYMBOL CAPSYMBOL
%token<constant> CONST
%token<character> CHAR
%token<string> STR
%token START_REGEXP
%token<integer> END_REGEXP
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%left WTF
%right ';'
%right FN
%nonassoc AS
%nonassoc LARROW
%nonassoc PUB MUT BUILTIN DEPRECATED
%nonassoc LIST_DDOT
%nonassoc DO
%nonassoc DEFINED
%nonassoc OPEN IMPORT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%right RARROW
%right LET REC AND ASSERT
%nonassoc RETURN
%left LAZY
%right ','
%right ELSE TERNARY
/*%nonassoc ELSE*/
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%nonassoc IF IFLET THEN TRY WITH
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// level 0:
%right '='
%right OR
%nonassoc ASSIGN
%left PIPE
%right '$'
// level 1:
%right OPOR
// level 2:
%right OPAND
// level 3:
%nonassoc ISOF
%left '<' LE '>' GE EQ NE IS ISNOT EQUAL NOTEQUAL DDOT DDDOTL DDDOTR DDDOT EQ_TILD
// level 4:
%right CONS PPLUS
// level 5:
%left '+' '-' MOD
// level 6:
%left '*' '/' LAND LOR LXOR
// level 7:
%right '^' LSHL LSHR ASHL ASHR
// level 8:
%left<string> USROP
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%left UMINUS UPLUS NOT LNOT
%right COMPOSE
%nonassoc APPLY
%left '.' ':' '%'


// =============================================================================
%type<ast> Atom Expr ExprSeq
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%type<binds> Binds
%type<bind> Bind
%type<integer> Attribute
%type<astvec> Args
%type<charvec> String
%type<ast> FmtString FmtStringAux
%type<ast> RegExp
%type<charvec> StringAux
%type<pattern> AtomicPattern ExprPattern
%type<astvec> List
%type<patvec> PatternList
%type<boolean> MaybeComaDots
%type<record> Record
%type<record_pattern> RecordPattern
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%start Entry


%%

Entry
  : ExprSeq { g_result = $1; }
  | START_REPL Expr { g_result = $2; }
;

Atom
  : SYMBOL { $$ = eth_ast_ident($1); free($1); LOC($$, @$); }
  /*| '(' Expr ')' { $$ = $2; }*/
  /*| Atom '!' { $$ = eth_ast_apply($1, NULL, 0); LOC($$, @$); }*/

  /*| '(' Expr DDOT Expr ')' { char *fields[] = { "l", "r" }; eth_ast *vals[] = { $2, $4 }; $$ = eth_ast_make_record(eth_rangelr_type, fields, vals, 2); }*/
  | '['']' { $$ = eth_ast_cval(eth_nil); }
  | '[' List ']' {
    $$ = eth_ast_cval(eth_nil);
    cod_vec_riter($2, i, x, $$ = eth_ast_binop(ETH_CONS, x, $$));
    cod_vec_destroy($2);
    LOC($$, @$);
  }
  | '#' '(' List ')' {
    int n = $3.len;
    char fieldsbuf[n][22];
    char *fields[n];
    for (int i = 0; i < n; ++i)
    {
      fields[i] = fieldsbuf[i];
      sprintf(fields[i], "_%d", i+1);
    }
    if ($3.len <= 1)
    {
      eth_error("invalid tuple");
      eth_location *loc = location(&@$);
      eth_print_location(loc, stderr);
      eth_drop_location(loc);
      abort();
    }
    $$ = eth_ast_make_record(eth_tuple_type(n), fields, $3.data, n);
    cod_vec_destroy($3);
  }
  | '#' '{'  Record '}' {
    eth_type *type = eth_record_type($3.keys.data, $3.keys.len);
    $$ = eth_ast_make_record(type, $3.keys.data, $3.vals.data, $3.keys.len);
    cod_vec_iter($3.keys, i, x, free(x));
    cod_vec_destroy($3.keys);
    cod_vec_destroy($3.vals);
  }

  | '#' '{' Expr WITH Record '}' {
    $$ = eth_ast_update($3, $5.vals.data, $5.keys.data, $5.vals.len);
    cod_vec_destroy($5.vals);
    cod_vec_iter($5.keys, i, x, free(x));
    cod_vec_destroy($5.keys);
  }

  | Atom '(' Args ')' %prec APPLY {
    if ($1->tag == ETH_AST_CVAL and
        $1->cval.val->type == eth_symbol_type)
    {
      // FIXME
      eth_type *type = eth_variant_type(eth_sym_cstr($1->cval.val));
      char *_0 = "_0";
      $$ = eth_ast_make_record(type, &_0, &$3.data[0], 1);
      eth_drop_ast($1);
      cod_vec_destroy($3);
      LOC($$, @$);
    }
    else
    {
      $$ = eth_ast_apply($1, $3.data, $3.len);
      LOC($$, @$);
      cod_vec_destroy($3);
    }
  }

  | Atom '.' SYMBOL {
    $$ = eth_ast_access($1, $3, false);
    LOC($$, @$);
    free($3);
  }
  | Atom ':' SYMBOL '(' Args ')' %prec APPLY {
    eth_ast *access = eth_ast_access($1, $3, true);
    LOC(access, @2);
    cod_vec_insert($5, $1, 0);
    $$ = eth_ast_apply(access, $5.data, $5.len);
    LOC($$, @$);
    free($3);
    cod_vec_destroy($5);
  }

  | '@' '(' Expr ')' { $$ = eth_ast_evmac($3); LOC($$, @$); }

  | '_' { $$ = eth_ast_ident("_"); }
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

  | '{' ExprSeq '}' { $$ = $2; }
;

Expr
  : Atom

  | DEFINED SYMBOL {
    $$ = eth_ast_defined($2);
    free($2);
    LOC($$, @$);
  }

  | Expr ISOF AtomicPattern {
    $$ = eth_ast_match($3, $1, eth_ast_cval(eth_true), eth_ast_cval(eth_false));
    LOC($$, @$);
  }

  | ASSERT Expr { $$ = eth_ast_assert($2); }

  /*| Form RARROW Expr {*/
    /*if ($1->tag == ETH_AST_APPLY)*/
    /*{*/
      /*int n = 1 + $1->apply.nargs;*/
      /*cod_vec(eth_ast_pattern*) pats;*/
      /*cod_vec_init_with_cap(pats, n);*/
      /*cod_vec_push(pats, eth_ast_to_pattern($1->apply.fn));*/
      /*for (int i = 1; i < n; ++i)*/
        /*cod_vec_push(pats, eth_ast_to_pattern($1->apply.args[i-1]));*/

      /*[> check if all expressions successfully converted <]*/
      /*bool failed = false;*/
      /*cod_vec_iter(pats, i, x, if (x == NULL) failed = true);*/

      /*if (failed)*/
      /*{*/
        /*eth_error("can not convert AST-expression to a pattern");*/
        /*abort();*/
      /*}*/
      /*$$ = eth_ast_fn_with_patterns(pats.data, n, $3);*/
      /*cod_vec_destroy(pats);*/
      /*eth_drop_ast($1);*/
      /*LOC($$, @$);*/
    /*}*/
    /*else*/
    /*{*/
      /*eth_ast_pattern *pat = eth_ast_to_pattern($1);*/
      /*if (pat == NULL)*/
      /*{*/
        /*eth_error("can not convert AST-expression to a pattern");*/
        /*abort();*/
      /*}*/
      /*$$ = eth_ast_fn_with_patterns(&pat, 1, $3);*/
      /*eth_drop_ast($1);*/
      /*LOC($$, @$);*/
    /*}*/
  /*}*/

  | Expr OR Expr { $$ = eth_ast_try(NULL, $1, $3, 0); LOC($$, @$); }
  | Expr OPAND Expr { $$ = eth_ast_and($1, $3); LOC($$, @$); }
  | Expr OPOR Expr { $$ = eth_ast_or($1, $3); LOC($$, @$); }
  | Expr '+' Expr { $$ = eth_ast_binop(ETH_ADD , $1, $3); LOC($$, @$); }
  | Expr '-' Expr { $$ = eth_ast_binop(ETH_SUB , $1, $3); LOC($$, @$); }
  | Expr '*' Expr { $$ = eth_ast_binop(ETH_MUL , $1, $3); LOC($$, @$); }
  | Expr '/' Expr { $$ = eth_ast_binop(ETH_DIV , $1, $3); LOC($$, @$); }
  | Expr '<' Expr { $$ = eth_ast_binop(ETH_LT  , $1, $3); LOC($$, @$); }
  | Expr LE  Expr { $$ = eth_ast_binop(ETH_LE  , $1, $3); LOC($$, @$); }
  | Expr '>' Expr { $$ = eth_ast_binop(ETH_GT  , $1, $3); LOC($$, @$); }
  | Expr GE Expr { $$ = eth_ast_binop(ETH_GE  , $1, $3); LOC($$, @$); }
  | Expr EQ Expr { $$ = eth_ast_binop(ETH_EQ  , $1, $3); LOC($$, @$); }
  | Expr NE Expr { $$ = eth_ast_binop(ETH_NE  , $1, $3); LOC($$, @$); }
  | Expr IS Expr { $$ = eth_ast_binop(ETH_IS  , $1, $3); LOC($$, @$); }
  | Expr EQUAL Expr { $$ = eth_ast_binop(ETH_EQUAL,$1, $3); LOC($$, @$); }
  | Expr CONS Expr { $$ = eth_ast_binop(ETH_CONS, $1, $3); LOC($$, @$); }
  | Expr MOD Expr { $$ = eth_ast_binop(ETH_MOD , $1, $3); LOC($$, @$); }
  | Expr '^' Expr { $$ = eth_ast_binop(ETH_POW , $1, $3); LOC($$, @$); }
  | Expr LAND Expr { $$ = eth_ast_binop(ETH_LAND, $1, $3); LOC($$, @$); }
  | Expr LOR Expr { $$ = eth_ast_binop(ETH_LOR , $1, $3); LOC($$, @$); }
  | Expr LXOR Expr { $$ = eth_ast_binop(ETH_LXOR, $1, $3); LOC($$, @$); }
  | Expr LSHL Expr { $$ = eth_ast_binop(ETH_LSHL, $1, $3); LOC($$, @$); }
  | Expr LSHR Expr { $$ = eth_ast_binop(ETH_LSHR, $1, $3); LOC($$, @$); }
  | Expr ASHL Expr { $$ = eth_ast_binop(ETH_ASHL, $1, $3); LOC($$, @$); }
  | Expr ASHR Expr { $$ = eth_ast_binop(ETH_ASHR, $1, $3); LOC($$, @$); }
  | Expr ISNOT Expr { $$ = eth_ast_unop(ETH_NOT, eth_ast_binop(ETH_IS, $1, $3)); LOC($$, @$); }
  | Expr NOTEQUAL Expr { $$ = eth_ast_unop(ETH_NOT, eth_ast_binop(ETH_EQUAL, $1, $3)); LOC($$, @$); }

  | Expr PPLUS Expr { eth_ast *args[] = { $1, $3 }; eth_ast *fn = eth_ast_ident("++"); $$ = eth_ast_apply(fn, args, 2); LOC($$, @$); }
  | Expr EQ_TILD Expr { eth_ast *args[] = { $1, $3 }; eth_ast *fn = eth_ast_ident("=~"); $$ = eth_ast_apply(fn, args, 2); LOC($$, @$); }
  | Expr DDOT Expr { char *fields[] = { "l", "r" }; eth_ast *vals[] = { $1, $3 }; $$ = eth_ast_make_record(eth_rangelr_type, fields, vals, 2); }
  | Expr DDDOTR  { char *fields[] = { "l" }; eth_ast *vals[] = { $1 }; $$ = eth_ast_make_record(eth_rangel_type, fields, vals, 1); }
  | DDDOTL Expr { char *fields[] = { "r" }; eth_ast *vals[] = { $2 }; $$ = eth_ast_make_record(eth_ranger_type, fields, vals, 1); }
  /*| '-' Expr %prec UMINUS { $$ = eth_ast_binop(ETH_SUB, eth_ast_cval(eth_num(0)), $2); LOC($$, @$); }*/
  /*| '+' Expr %prec UPLUS { $$ = eth_ast_binop(ETH_ADD, eth_ast_cval(eth_num(0)), $2); LOC($$, @$); }*/

  | NOT Expr { $$ = eth_ast_unop(ETH_NOT , $2); LOC($$, @$); }
  | LNOT Expr { $$ = eth_ast_unop(ETH_LNOT, $2); LOC($$, @$); }

  | Expr USROP Expr { eth_ast *args[] = { $1, $3 }; eth_ast *fn = eth_ast_ident($2); $$ = eth_ast_apply(fn, args, 2); LOC($$, @$); free($2); }

  | SYMBOL ASSIGN Expr { $$ = eth_ast_assign($1, $3); free($1); LOC($$, @$); }

  | IF Expr THEN Expr ELSE Expr %prec ELSE { $$ = eth_ast_if($2, $4, $6); LOC($$, @$); }
  | IF Expr THEN Expr %prec TERNARY { $$ = eth_ast_if($2, $4, eth_ast_cval(eth_nil)); LOC($$, @$); }

  | IFLET ExprPattern '=' Expr THEN Expr ELSE Expr %prec ELSE { $$ = eth_ast_match($2, $4, $6, $8); LOC($$, @$); }
  | IFLET ExprPattern '=' Expr THEN Expr %prec TERNARY { $$ = eth_ast_match($2, $4, $6, eth_ast_cval(eth_nil)); LOC($$, @$); }

  | LET Binds {
    $$ = eth_ast_let($2.pats.data, $2.vals.data, $2.pats.len, eth_ast_cval(eth_nil));
    cod_vec_destroy($2.pats);
    cod_vec_destroy($2.vals);
    LOC($$, @$);
  }

  | LET REC Binds {
    $$ = eth_ast_letrec($3.pats.data, $3.vals.data, $3.pats.len, eth_ast_cval(eth_nil));
    cod_vec_destroy($3.pats);
    cod_vec_destroy($3.vals);
    LOC($$, @$);
  }

  | TRY Expr WITH ExprPattern RARROW Expr %prec TRY { $$ = eth_ast_try($4, $2, $6, 1); LOC($$, @$); }

  | LAZY Expr {
    eth_t lazy = eth_get_builtin(SCANROOT, "__make_lazy");
    assert(lazy);
    eth_ast *thunk = eth_ast_fn(NULL, 0, $2);
    $$ = eth_ast_apply(eth_ast_cval(lazy), &thunk, 1);
  }

  | FN '(' PatternList ')' Expr %prec FN {
    MESSAGE(warning, @$, "deprecated syntax: `fn (<args>) <expr>`");
    $$ = eth_ast_fn_with_patterns($3.data, $3.len, $5);
    cod_vec_destroy($3);
    LOC($$, @$);
  }
  | FN PatternList RARROW Expr %prec FN {
    $$ = eth_ast_fn_with_patterns($2.data, $2.len, $4);
    cod_vec_destroy($2);
    LOC($$, @$);
  }

  | Expr PIPE Expr { if ($3->tag == ETH_AST_APPLY) { $$ = $3; eth_ast_append_arg($$, $1); } else $$ = eth_ast_apply($3, &$1, 1); LOC($$, @$); }
  | Expr '$' Expr { if ($1->tag == ETH_AST_APPLY) { $$ = $1; eth_ast_append_arg($$, $3); } else $$ = eth_ast_apply($1, &$3, 1); LOC($$, @$); }

  | Attribute OPEN Atom {
    eth_ast *rhs;
    if ($3->tag == ETH_AST_CVAL and $3->cval.val->type == eth_string_type)
    {
      eth_ast *require = eth_ast_cval(eth_get_builtin(SCANROOT, "__require"));
      rhs = eth_ast_evmac(eth_ast_apply(require, &$3, 1));
    }
    else
      rhs = $3;
    eth_ast_pattern *lhs = eth_ast_record_star_pattern();
    eth_ref_attr(lhs->recordstar.attr = eth_create_attr($1));
    $$ = eth_ast_let(&lhs, &rhs, 1, eth_ast_cval(eth_nil));
  }


  | Attribute IMPORT String {
    cod_vec_push($3, 0);
    eth_ast *require = eth_ast_cval(eth_get_builtin(SCANROOT, "__require"));
    eth_ast_pattern *lhs = eth_ast_ident_pattern($3.data);
    eth_ref_attr(lhs->ident.attr = eth_create_attr($1));
    eth_ast *arg = eth_ast_cval(eth_create_string_from_ptr2($3.data, $3.len - 1));
    eth_ast *rhs = eth_ast_evmac(eth_ast_apply(require, &arg, 1));
    $3.data = NULL;
    $$ = eth_ast_let(&lhs, &rhs, 1, eth_ast_cval(eth_nil));
  }
  /*| Attribute IMPORT Expr AS SYMBOL {*/
    /*eth_ast *rhs;*/
    /*if ($3->tag == ETH_AST_CVAL and $3->cval.val->type == eth_string_type)*/
    /*{*/
      /*eth_ast *require = eth_ast_cval(eth_get_builtin(SCANROOT, "__require"));*/
      /*rhs = eth_ast_evmac(eth_ast_apply(require, &$3, 1));*/
    /*}*/
    /*else*/
      /*rhs = $3;*/
    /*eth_ast_pattern *lhs = eth_ast_ident_pattern($5);*/
    /*eth_ref_attr(lhs->ident.attr = eth_create_attr($1));*/
    /*free($5);*/
    /*$$ = eth_ast_let(&lhs, &rhs, 1, eth_ast_cval(eth_nil));*/
  /*}*/

  | RETURN Expr { $$ = eth_ast_return($2); LOC($$, @$); }
;

ExprSeq
  : Expr
  | Expr ExprSeq %prec ';' {
    if (($1->tag == ETH_AST_LET || $1->tag == ETH_AST_LETREC))
    {
      eth_unref_ast($1->let.body);
      eth_ref_ast($1->let.body = $2);
      $$ = $1;
    }
    else
      $$ = eth_ast_seq($1, $2);
    LOC($$, @$);
  }
;

Args
  : { cod_vec_init($$); }
  | Expr {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | Args ',' Expr {
    $$ = $1;
    cod_vec_push($$, $3);
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
  : ExprPattern '=' Expr {
    $$.pat = $1;
    $$.val = $3;
    if ($1->tag == ETH_AST_PATTERN_IDENT &&
        $1->ident.attr &&
        $1->ident.attr->flag & ETH_ATTR_MUT)
    {
      eth_t mkref = eth_get_builtin(SCANROOT, "__create_ref");
      eth_ast *p[] = { $$.val };
      $$.val = eth_ast_apply(eth_ast_cval(mkref), p, 1);
    }
  }
  | Attribute SYMBOL '(' PatternList ')' '{' ExprSeq '}' {
    $$.pat = eth_ast_ident_pattern($2);
    eth_attr *attr = eth_create_attr($1);
    if (g_filename)
      eth_set_location(attr, location(&@$));
    eth_ref_attr($$.pat->ident.attr = attr);
    $$.val = eth_ast_fn_with_patterns($4.data, $4.len, $7);
    free($2);
    cod_vec_destroy($4);
  }
;

Attribute
  : { $$ = 0; }
  | Attribute PUB        { $$ = $1 | ETH_ATTR_PUB;        }
  | Attribute MUT        { $$ = $1 | ETH_ATTR_MUT;        }
  | Attribute BUILTIN    { $$ = $1 | ETH_ATTR_BUILTIN;    }
  | Attribute DEPRECATED { $$ = $1 | ETH_ATTR_DEPRECATED; }
;

String
  : '"' StringAux '"' { $$ = $2; }
;

FmtString
  : '"' FmtStringAux '"' {
    eth_ast *fmt = eth_ast_cval(eth_get_builtin(SCANROOT, "__fmt"));
    eth_ast *p[1] = { $2 };
    $$ = eth_ast_apply(fmt, p, 1);
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
  | StringAux STR {
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
  | '['']' { $$ = eth_ast_constant_pattern(eth_nil); }
  | '{' ExprPattern '}' { $$ = $2; }
  | '#' '(' PatternList  ')' {
    if ($3.len < 1)
    {
      eth_error("invalid tuple");
      eth_location *loc = location(&@$);
      eth_print_location(loc, stderr);
      eth_drop_location(loc);
      abort();
    }

    int n = $3.len;
    char fieldsbuf[n][22];
    char *fields[n];
    for (int i = 0; i < n; ++i)
    {
      fields[i] = fieldsbuf[i];
      sprintf(fields[i], "_%d", i+1);
    }
    $$ = eth_ast_record_pattern(fields, $3.data, n);

    cod_vec_destroy($3);
  }
  | '#' '{' RecordPattern '}' {
    $$ = eth_ast_record_pattern($3.keys.data, $3.vals.data, $3.keys.len);
    cod_vec_iter($3.keys, i, x, free(x));
    cod_vec_destroy($3.keys);
    cod_vec_destroy($3.vals);
  }
  | '#' '{' Attribute '*' '}' {
    $$ = eth_ast_record_star_pattern();
    eth_attr *attr = eth_create_attr($3);
    if (g_filename)
      eth_set_location(attr, location(&@$));
    eth_ref_attr($$->recordstar.attr = attr);
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

  | CAPSYMBOL '(' ExprPattern ')' {
    eth_type *type = eth_variant_type($1);
    char *_0 = "_0";
    $$ = eth_ast_unpack_pattern(type, &_0, &$3, 1);
    free($1);
  }

;

MaybeComaDots
  : { $$ = false; }
  | DDDOT { $$ = true; }
;

ExprPattern
  : AtomicPattern
  | ExprPattern CONS ExprPattern {
    char *fields[2] = { "car", "cdr" };
    eth_ast_pattern *pats[2] = { $1, $3 };
    $$ = eth_ast_unpack_pattern(eth_pair_type, fields, pats, 2);
  }
  | AtomicPattern DDOT AtomicPattern {
    char *fields[2] = { "l", "r" };
    eth_ast_pattern *pats[2] = { $1, $3 };
    $$ = eth_ast_unpack_pattern(eth_rangelr_type, fields, pats, 2);
  }
  | ExprPattern AS SYMBOL {
    $$ = $1;
    eth_set_pattern_alias($$, $3);
    free($3);
  }
  |  ExprPattern DDDOTR {
    char *fields[1] = { "l" };
    eth_ast_pattern *pats[1] = { $1 };
    $$ = eth_ast_unpack_pattern(eth_rangel_type, fields, pats, 1);
  }
  | DDDOTL ExprPattern {
    char *fields[1] = { "r" };
    eth_ast_pattern *pats[1] = { $2 };
    $$ = eth_ast_unpack_pattern(eth_ranger_type, fields, pats, 1);
  }
;

PatternList
  : { cod_vec_init($$); }
  | PatternList ExprPattern {
    $$ = $1;
    cod_vec_push($$, $2);
  }
  | PatternList ',' { $$ = $1; }
;

RecordPattern
  /*: SYMBOL {*/
    /*cod_vec_init($$.keys);*/
    /*cod_vec_init($$.vals);*/
    /*cod_vec_push($$.keys, $1);*/
    /*cod_vec_push($$.vals, eth_ast_ident_pattern($1));*/
  /*}*/
  : Attribute SYMBOL {
    cod_vec_init($$.keys); cod_vec_init($$.vals);
    eth_ast_pattern *idpat = eth_ast_ident_pattern($2);
    eth_set_ident_attr(idpat, create_attr($1, @2));
    cod_vec_push($$.keys, $2);
    cod_vec_push($$.vals, idpat);
  }
  | Attribute SYMBOL '=' ExprPattern {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $2);
    cod_vec_push($$.vals, $4);
  }
  /*| RecordPattern ',' SYMBOL {*/
    /*$$ = $1;*/
    /*cod_vec_push($$.keys, $3);*/
    /*cod_vec_push($$.vals, eth_ast_ident_pattern($3));*/
  /*}*/
  | RecordPattern Attribute SYMBOL {
    $$ = $1;
    eth_ast_pattern *idpat = eth_ast_ident_pattern($3);
    eth_set_ident_attr(idpat, create_attr($2, @3));
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, idpat);
  }
  | RecordPattern Attribute SYMBOL '=' ExprPattern {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, $5);
  }
  | RecordPattern ',' { $$ = $1; }
;

List
  : Expr {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | List Expr {
    $$ = $1;
    cod_vec_push($$, $2);
  }
  | List ',' { $$ = $1; }
;

Record
  : SYMBOL '=' Expr {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, $3);
  }
  | SYMBOL '(' PatternList ')' Expr {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);

    eth_ast *fn = eth_ast_fn_with_patterns($3.data, $3.len, $5);
    cod_vec_destroy($3);

    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, fn);
  }
  | SYMBOL {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, eth_ast_ident($1));
  }
  | Record SYMBOL '=' Expr {
    $$ = $1;
    cod_vec_push($$.keys, $2);
    cod_vec_push($$.vals, $4);
  }
  | Record SYMBOL {
    $$ = $1;
    cod_vec_push($$.keys, $2);
    cod_vec_push($$.vals, eth_ast_ident($2));
  }
  | Record SYMBOL '(' PatternList ')' Expr {
    $$ = $1;

    eth_ast *fn = eth_ast_fn_with_patterns($4.data, $4.len, $6);
    cod_vec_destroy($4);

    cod_vec_push($$.keys, $2);
    cod_vec_push($$.vals, fn);
  }
  | Record ',' { $$ = $1; }
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
eth_parse(eth_root *root, FILE *stream)
{
  eth_scanner *scan = eth_create_scanner(root, stream);

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
eth_parse_repl(eth_scanner *scan)
{
  cod_vec_push(eth_get_scanner_data(scan)->primtoks, START_REPL);

  g_result = NULL;
  g_iserror = false;
  g_filename = filename(eth_get_scanner_input(scan));
  yyparse(scan);

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

  const char eofstr[] = "syntax error, unexpected end of file";
  if (not eth_is_repl_scanner(yyscanner) or strncmp(what, eofstr, sizeof eofstr - 1) != 0)
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

