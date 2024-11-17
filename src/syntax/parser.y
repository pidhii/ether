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
    cod_vec(eth_ast*) keys;
    cod_vec(eth_ast*) vals;
  } dict;
  struct {
    cod_vec(char*) keys;
    cod_vec(eth_ast_pattern*) vals;
  } record_pattern;
  struct {
    cod_vec(char*) fldnams;
    cod_vec(eth_ast*) fldvals;
    cod_vec(eth_ast*) methods;
    cod_vec(eth_ast*) methimpls;
    cod_vec(int) mutfields;
  } structure;

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
  cod_vec_iter($$.keys, i, x, eth_drop_ast(x));
  cod_vec_iter($$.vals, i, x, eth_drop_ast(x));
  cod_vec_destroy($$.keys);
  cod_vec_destroy($$.vals);
} <dict>


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

%destructor {
  cod_vec_iter($$.fldnams, i, x, free(x));
  cod_vec_iter($$.fldvals, i, x, eth_drop_ast(x));
  cod_vec_iter($$.methods, i, x, eth_drop_ast(x));
  cod_vec_iter($$.methimpls, i, x, eth_drop_ast(x));
  cod_vec_destroy($$.fldnams);
  cod_vec_destroy($$.fldvals);
  cod_vec_destroy($$.methods);
  cod_vec_destroy($$.methimpls);
  cod_vec_destroy($$.mutfields);
} <structure>

// =============================================================================
%token START_REPL UNDEFINED
%token START_FORMAT END_FORMAT
%token STRUCT ENUM
%token THIS
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%token<number> NUMBER
%token<string> SYMBOL
%token<constant> CONST
%token<character> CHAR
%token<string> STR
%token START_REGEXP
%token<integer> END_REGEXP
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%right FN
%right IN
%right ';'
%nonassoc AS
%nonassoc LARROW
%nonassoc PUB MUT BUILTIN DEPRECATED
%nonassoc LIST_DDOT
%nonassoc DO
%nonassoc DEFINED
%nonassoc OPEN IMPORT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%right RARROW
%right LET REC AND ASSERT METHOD IMPL VAL
%nonassoc RETURN
%left LAZY
%right ','
%right ELSE TERNARY
/*%nonassoc ELSE*/
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%nonassoc IF IFLET THEN TRY EXCEPT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// level 0:
/*%right '='*/
%right OR
%nonassoc ASSIGN '='
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
%nonassoc WITH
%right COMPOSE
%nonassoc GROUPING
%nonassoc APPLY
%left '.' ':' '%'


// =============================================================================
%type<ast> Atom Form Expr Tuple
%type<astvec> TupleAux
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%type<binds> Binds
%type<bind> Bind
%type<integer> Attribute
%type<astvec> Args
%type<charvec> String
%type<ast> FmtString FmtStringAux
%type<ast> RegExp
%type<charvec> StringAux
%type<pattern> AtomicPattern FormPattern ExprPattern
%type<astvec> List
%type<patvec> ArgPatterns ArgPatternsPlus TuplePatternAux
%type<boolean> MaybeComaDots
%type<record> Record
%type<dict> Dict
/*%type<record_or_dict> RecordOrDict*/
%type<record_pattern> RecordPattern
%type<structure> Struct
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
%start Entry


%%

Entry
  : Expr { g_result = $1; }
  | START_REPL Expr { g_result = $2; }
;

Atom
  : SYMBOL { $$ = eth_ast_ident($1); free($1); LOC($$, @$); }
  /*| '(' Expr DDOT Expr ')' { char *fields[] = { "l", "r" }; eth_ast *vals[] = { $2, $4 }; $$ = eth_ast_make_record(eth_rangelr_type, fields, vals, 2); }*/
  | THIS { $$ = eth_ast_this(); }
  | '['']' { $$ = eth_ast_cval(eth_nil); }
  | '[' List ']' {
    $$ = eth_ast_cval(eth_nil);
    cod_vec_riter($2, i, x, $$ = eth_ast_binop(ETH_CONS, x, $$));
    cod_vec_destroy($2);
    LOC($$, @$);
  }
  | '{' Record '}' {
    eth_type *type = eth_record_type($2.keys.data, $2.keys.len);
    $$ = eth_ast_make_record(type, $2.keys.data, $2.vals.data, $2.keys.len);
    cod_vec_iter($2.keys, i, x, free(x));
    cod_vec_destroy($2.keys);
    cod_vec_destroy($2.vals);
  }
  | '{' Dict '}' {
    eth_ast *rbtree = eth_ast_cval(eth_get_builtin(SCANROOT, "dict"));
    eth_ast *set = eth_ast_cval(eth_set_method);
    for (size_t i = 0; i < $2.keys.len; ++i)
    {
      eth_ast *p[] = {rbtree, $2.keys.data[i], $2.vals.data[i]};
      rbtree = eth_ast_apply(set, p, 3);
    }
    $$ = rbtree;
    cod_vec_destroy($2.keys);
    cod_vec_destroy($2.vals);
  }

  | Tuple

  | Atom '.' SYMBOL {
    $$ = eth_ast_access($1, $3);
    LOC($$, @$);
    free($3);
  }
  | Atom '.' String {
    cod_vec_push($3, 0);
    $$ = eth_ast_access($1, $3.data);
    LOC($$, @$);
    cod_vec_destroy($3);
  }
  /*| Atom ':' SYMBOL {*/
    /*eth_ast *access = eth_ast_access($1, $3);*/
    /*LOC(access, @2);*/
    /*$$ = eth_ast_apply(access, &$1, 1);*/
    /*LOC($$, @$);*/
    /*free($3);*/
  /*}*/

  | Atom '.' '(' Expr ')' {
    eth_ast *p[] = { $1, $4 };
    $$ = eth_ast_apply(eth_ast_cval(eth_get_method), p, 2);
    LOC($$, @$);
  }

  | '@' '(' Expr ')' { $$ = eth_ast_evmac($3); LOC($$, @$); }

  | '_' { $$ = eth_ast_ident("_"); }
  | NUMBER { $$ = eth_ast_cval(eth_create_number($1)); LOC($$, @$); }
  | CONST  { $$ = eth_ast_cval($1); LOC($$, @$); }
  | String {
    cod_vec_push($1, 0);
    $$ = eth_ast_cval(eth_create_string_from_ptr2($1.data, $1.len - 1));
    LOC($$, @$);
  }
  | FmtString
  | RegExp

  | ENUM '(' TupleAux ')' {
    int n = $3.len;
    char fieldsbuf[n][22];
    char *fields[n];
    for (int i = 0; i < n; ++i)
    {
      fields[i] = fieldsbuf[i];
      sprintf(fields[i], "_%d", i+1);
    }
    eth_type *type = eth_unique_record_type(fields, n);
    eth_add_method(type->methods, eth_enum_ctor_method, eth_nil);
    $$ = eth_ast_make_record(type, fields, $3.data, n);
    cod_vec_destroy($3);
    LOC($$, @$);
  }
;

Form
  : Atom
  | Atom Args {
    $$ = eth_ast_apply($1, $2.data, $2.len);
    LOC($$, @$);
    cod_vec_destroy($2);
  }
;

Expr
  : Form

  | Expr WITH '{' Record '}' {
    $$ = eth_ast_update($1, $4.vals.data, $4.keys.data, $4.vals.len);
    cod_vec_iter($4.keys, i, x, free(x));
    cod_vec_destroy($4.keys);
    cod_vec_destroy($4.vals);
  }

  | Expr ';'
  | Expr ';' Expr {
    if (($1->tag == ETH_AST_LET || $1->tag == ETH_AST_LETREC))
    {
      eth_unref_ast($1->let.body);
      eth_ref_ast($1->let.body = $3);
      $$ = $1;
    }
    else
      $$ = eth_ast_seq($1, $3);
    LOC($$, @$);
  }

  | Atom '.' '(' Expr ')' LARROW Expr {
    eth_ast *p[] = { $1, $4, $7 };
    $$ = eth_ast_apply(eth_ast_cval(eth_set_method), p, 3);
    LOC($$, @$);
  }


  | DEFINED SYMBOL {
    $$ = eth_ast_defined($2);
    free($2);
    LOC($$, @$);
  }

  | Expr ISOF FormPattern {
    $$ = eth_ast_match($3, $1, eth_ast_cval(eth_true), eth_ast_cval(eth_false));
    LOC($$, @$);
  }

  | ASSERT Expr { $$ = eth_ast_assert($2); }

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
  | '-' Expr %prec UMINUS { $$ = eth_ast_binop(ETH_SUB, eth_ast_cval(eth_num(0)), $2); LOC($$, @$); }
  | '+' Expr %prec UPLUS { $$ = eth_ast_binop(ETH_ADD, eth_ast_cval(eth_num(0)), $2); LOC($$, @$); }

  | NOT Expr { $$ = eth_ast_unop(ETH_NOT , $2); LOC($$, @$); }
  | LNOT Expr { $$ = eth_ast_unop(ETH_LNOT, $2); LOC($$, @$); }

  | Expr USROP Expr { eth_ast *args[] = { $1, $3 }; eth_ast *fn = eth_ast_ident($2); $$ = eth_ast_apply(fn, args, 2); LOC($$, @$); free($2); }
  /*| USROP Expr %prec UMINUS {*/
    /*eth_ast *args[] = { $2 };*/
    /*eth_ast *fn = eth_ast_ident($1);*/
    /*$$ = eth_ast_apply(fn, args, 1);*/
    /*LOC($$, @$);*/
    /*free($1);*/
  /*}*/

  | SYMBOL ASSIGN Expr { $$ = eth_ast_assign($1, $3); free($1); LOC($$, @$); }
  | SYMBOL '=' Expr { $$ = eth_ast_assign($1, $3); free($1); LOC($$, @$); }
  | Atom '.' SYMBOL '=' Expr {
    eth_t assign_field = eth_get_builtin(SCANROOT, "__assign_field");
    assert(assign_field);
    eth_ast *p[] = { $1, eth_ast_cval(eth_sym($3)), $5 };
    $$ = eth_ast_apply(eth_ast_cval(assign_field), p, 3);
    LOC($$, @$);
    free($3);
  }

  | IF Expr THEN Expr ELSE Expr %prec ELSE { $$ = eth_ast_if($2, $4, $6); LOC($$, @$); }
  | IF Expr THEN Expr %prec TERNARY { $$ = eth_ast_if($2, $4, eth_ast_cval(eth_nil)); LOC($$, @$); }

  | IFLET ExprPattern '=' Expr THEN Expr ELSE Expr %prec ELSE { $$ = eth_ast_match($2, $4, $6, $8); LOC($$, @$); }
  | IFLET ExprPattern '=' Expr THEN Expr %prec TERNARY { $$ = eth_ast_match($2, $4, $6, eth_ast_cval(eth_nil)); LOC($$, @$); }

  | LET Binds IN Expr %prec ';' {
    $$ = eth_ast_let($2.pats.data, $2.vals.data, $2.pats.len, $4);
    cod_vec_destroy($2.pats);
    cod_vec_destroy($2.vals);
    LOC($$, @$);
  }

  | LET REC Binds IN Expr %prec ';' {
    $$ = eth_ast_letrec($3.pats.data, $3.vals.data, $3.pats.len, $5);
    cod_vec_destroy($3.pats);
    cod_vec_destroy($3.vals);
    LOC($$, @$);
  }

  | TRY Expr EXCEPT ExprPattern RARROW Expr %prec TRY { $$ = eth_ast_try($4, $2, $6, 1); LOC($$, @$); }

  | LAZY Expr {
    eth_t lazy = eth_get_builtin(SCANROOT, "__make_lazy");
    assert(lazy);
    eth_ast *thunk = eth_ast_fn(NULL, 0, $2);
    $$ = eth_ast_apply(eth_ast_cval(lazy), &thunk, 1);
  }

  | FN ArgPatterns RARROW Expr %prec FN {
    $$ = eth_ast_fn_with_patterns($2.data, $2.len, $4);
    cod_vec_destroy($2);
    LOC($$, @$);
  }

  | FN RARROW Expr %prec FN {
    $$ = eth_ast_fn_with_patterns(NULL, 0, $3);
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

  | IMPORT String {
    cod_vec_push($2, 0);
    eth_ast *require = eth_ast_cval(eth_get_builtin(SCANROOT, "__require"));
    eth_ast *arg = eth_ast_cval(eth_create_string_from_ptr2($2.data, $2.len - 1));
    $2.data = NULL;
    $$ = eth_ast_evmac(eth_ast_apply(require, &arg, 1));
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

  | METHOD ArgPatterns RARROW Expr {
    eth_t make_method = eth_get_builtin(SCANROOT, "__make_method");
    eth_ast *impl = eth_ast_fn_with_patterns($2.data, $2.len, $4);
    eth_ast *p[] = { eth_ast_cval(eth_num($2.len)), impl };
    $$ = eth_ast_apply(eth_ast_cval(make_method), p, 2);
    cod_vec_destroy($2);
  }

  | METHOD ArgPatterns RARROW UNDEFINED {
    eth_t make_method = eth_get_builtin(SCANROOT, "__make_method");
    eth_ast *p[] = { eth_ast_cval(eth_num($2.len)), eth_ast_cval(eth_nil) };
    $$ = eth_ast_apply(eth_ast_cval(make_method), p, 2);
    cod_vec_iter($2, i, x, eth_drop_ast_pattern(x));
    cod_vec_destroy($2);
  }

  | STRUCT Struct {
    // TODO: can set fields inside __make_struct (no need for extra update)
    eth_t fldnams = eth_nil;
    int n = $2.fldnams.len;
    for (int i = n - 1; i >= 0; --i)
      fldnams = eth_cons(eth_str($2.fldnams.data[i]), fldnams);
    eth_ast *methods = eth_ast_cval(eth_nil);
    n = $2.methods.len;
    for (int i = n - 1; i >= 0; --i)
    {
      char *fields[] = { "_1", "_2" };
      eth_ast *vals[] = { $2.methods.data[i], $2.methimpls.data[i] };
      eth_ast *meth = eth_ast_make_record(eth_tuple_type(2), fields, vals, 2);
      methods = eth_ast_binop(ETH_CONS, meth, methods);
    }
    eth_ast *mutfields = eth_ast_cval(eth_nil);
    for (int i = $2.mutfields.len - 1; i >= 0; --i)
    {
      eth_ast *fieldno = eth_ast_cval(eth_num($2.mutfields.data[i]));
      mutfields = eth_ast_binop(ETH_CONS, fieldno, mutfields);
    }
    eth_ast *p[] = { eth_ast_cval(fldnams), methods, mutfields };
    eth_t make_struct = eth_get_builtin(SCANROOT, "__make_struct");
    eth_ast *proto = eth_ast_apply(eth_ast_cval(make_struct), p, 3);

    $$ = eth_ast_update(proto, $2.fldvals.data, $2.fldnams.data, $2.fldnams.len);

    cod_vec_iter($2.fldnams, i, x, free(x));
    cod_vec_destroy($2.fldnams);
    cod_vec_destroy($2.fldvals);
    cod_vec_destroy($2.methods);
    cod_vec_destroy($2.methimpls);
    cod_vec_destroy($2.mutfields);
  }

;

Tuple
  : '(' TupleAux ')' {
    if ($2.len == 1)
    {
      $$ = $2.data[0];
      cod_vec_destroy($2);
    }
    else
    {
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
      LOC($$, @$);
    }
  }
;

TupleAux
  : Expr {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | TupleAux ',' Expr {
    $$ = $1;
    cod_vec_push($$, $3);
  }
;

Args
  : '(' ')' { cod_vec_init($$); }
  | Atom {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | Args Atom {
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
  | Attribute FN SYMBOL ArgPatterns '=' Expr {
    $$.pat = eth_ast_ident_pattern($3);
    eth_attr *attr = eth_create_attr($1);
    if (g_filename) eth_set_location(attr, location(&@$));
    eth_ref_attr($$.pat->ident.attr = attr);
    $$.val = eth_ast_fn_with_patterns($4.data, $4.len, $6);
    free($3);
    cod_vec_destroy($4);
  }

  | Attribute METHOD SYMBOL ArgPatterns '=' Expr {
    $$.pat = eth_ast_ident_pattern($3);
    eth_attr *attr = eth_create_attr($1);
    if (g_filename) eth_set_location(attr, location(&@$));
    eth_ref_attr($$.pat->ident.attr = attr);
    eth_t make_method = eth_get_builtin(SCANROOT, "__make_method");
    eth_ast *impl = eth_ast_fn_with_patterns($4.data, $4.len, $6);
    eth_ast *p[] = { eth_ast_cval(eth_num($4.len)), impl };
    $$.val = eth_ast_apply(eth_ast_cval(make_method), p, 2);
    free($3);
    cod_vec_destroy($4);
  }

  | Attribute METHOD SYMBOL ArgPatterns '=' UNDEFINED {
    $$.pat = eth_ast_ident_pattern($3);
    eth_attr *attr = eth_create_attr($1);
    if (g_filename) eth_set_location(attr, location(&@$));
    eth_ref_attr($$.pat->ident.attr = attr);
    eth_t make_method = eth_get_builtin(SCANROOT, "__make_method");
    eth_ast *p[] = { eth_ast_cval(eth_num($4.len)), eth_ast_cval(eth_nil) };
    $$.val = eth_ast_apply(eth_ast_cval(make_method), p, 2);
    free($3);
    cod_vec_iter($4, i, x, eth_drop_ast_pattern(x));
    cod_vec_destroy($4);
  }

  | Attribute STRUCT SYMBOL '=' Struct {
    $$.pat = eth_ast_ident_pattern($3);
    eth_attr *attr = eth_create_attr($1);
    if (g_filename) eth_set_location(attr, location(&@$));
    eth_ref_attr($$.pat->ident.attr = attr);
    eth_t fldnams = eth_nil;
    int n = $5.fldnams.len;
    for (int i = n - 1; i >= 0; --i)
      fldnams = eth_cons(eth_str($5.fldnams.data[i]), fldnams);
    eth_ast *methods = eth_ast_cval(eth_nil);
    n = $5.methods.len;
    for (int i = n - 1; i >= 0; --i)
    {
      char *fields[] = { "_1", "_2" };
      eth_ast *vals[] = { $5.methods.data[i], $5.methimpls.data[i] };
      eth_ast *meth = eth_ast_make_record(eth_tuple_type(2), fields, vals, 2);
      methods = eth_ast_binop(ETH_CONS, meth, methods);
    }
    eth_ast *mutfields = eth_ast_cval(eth_nil);
    for (int i = $5.mutfields.len - 1; i >= 0; --i)
    {
      eth_ast *fieldno = eth_ast_cval(eth_num($5.mutfields.data[i]));
      mutfields = eth_ast_binop(ETH_CONS, fieldno, mutfields);
    }
    eth_ast *p[] = { eth_ast_cval(fldnams), methods, mutfields };
    eth_t make_struct = eth_get_builtin(SCANROOT, "__make_struct");
    eth_ast *proto = eth_ast_apply(eth_ast_cval(make_struct), p, 3);

    $$.val = eth_ast_update(proto, $5.fldvals.data, $5.fldnams.data, $5.fldnams.len);

    free($3);
    cod_vec_iter($5.fldnams, i, x, free(x));
    cod_vec_destroy($5.fldnams);
    cod_vec_destroy($5.fldvals);
    cod_vec_destroy($5.methods);
    cod_vec_destroy($5.methimpls);
    cod_vec_destroy($5.mutfields);
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
  | CONST { $$ = eth_ast_constant_pattern($1); }
  | String {
    cod_vec_push($1, 0);
    eth_t str = eth_create_string_from_ptr2($1.data, $1.len - 1);
    $$ = eth_ast_constant_pattern(str);
  }
  | NUMBER { $$ = eth_ast_constant_pattern(eth_create_number($1)); }
  | '['']' { $$ = eth_ast_constant_pattern(eth_nil); }
  | '(' TuplePatternAux ')' {
    if ($2.len == 1)
    {
      $$ = $2.data[0];
    }
    else
    {
      int n = $2.len;
      char fieldsbuf[n][22];
      char *fields[n];
      for (int i = 0; i < n; ++i)
      {
        fields[i] = fieldsbuf[i];
        sprintf(fields[i], "_%d", i+1);
      }
      $$ = eth_ast_record_pattern(NULL, fields, $2.data, n);
    }
    cod_vec_destroy($2);
  }
  | '{' RecordPattern '}' {
    $$ = eth_ast_record_pattern(NULL, $2.keys.data, $2.vals.data, $2.keys.len);
    cod_vec_iter($2.keys, i, x, free(x));
    cod_vec_destroy($2.keys);
    cod_vec_destroy($2.vals);
  }
  | '{' Attribute '*' '}' {
    $$ = eth_ast_record_star_pattern();
    eth_attr *attr = eth_create_attr($2);
    if (g_filename)
      eth_set_location(attr, location(&@$));
    eth_ref_attr($$->recordstar.attr = attr);
  }
  | '[' TuplePatternAux MaybeComaDots ']' { // FIXME dots dont work
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
  | DDDOT { $$ = true; }
;

FormPattern
  : AtomicPattern
  | AtomicPattern AtomicPattern {
    if ($1->tag != ETH_AST_PATTERN_IDENT)
    {
      eth_error("invalid syntax: expected identifier, got ...");
      abort();
    }
    $$ = eth_ast_record_pattern(NULL, &$1->ident.str, &$2, 1);
    eth_drop_ast_pattern($1);
  }
;

ExprPattern
  : FormPattern
  | ExprPattern CONS ExprPattern {
    char *fields[2] = { "car", "cdr" };
    eth_ast_pattern *pats[2] = { $1, $3 };
    $$ = eth_ast_unpack_pattern(eth_pair_type, fields, pats, 2);
  }
  | ExprPattern DDOT ExprPattern {
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

TuplePatternAux
  : ExprPattern {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | TuplePatternAux ',' ExprPattern {
    $$ = $1;
    cod_vec_push($$, $3);
  }
;

ArgPatterns
  : { cod_vec_init($$); }
  | ArgPatterns AtomicPattern {
    $$ = $1;
    cod_vec_push($$, $2);
  }
;

ArgPatternsPlus
  : AtomicPattern {
    cod_vec_init($$);
    cod_vec_push($$, $1);
  }
  | ArgPatternsPlus AtomicPattern {
    $$ = $1;
    cod_vec_push($$, $2);
  }
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
  | Attribute SYMBOL '=' AtomicPattern {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $2);
    cod_vec_push($$.vals, $4);
  }
  | RecordPattern Attribute SYMBOL {
    $$ = $1;
    eth_ast_pattern *idpat = eth_ast_ident_pattern($3);
    eth_set_ident_attr(idpat, create_attr($2, @3));
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, idpat);
  }
  | RecordPattern Attribute SYMBOL '=' AtomicPattern {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, $5);
  }
  | RecordPattern ','
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
  | List ','
;

Record
  /*: {*/
    /*cod_vec_init($$.keys);*/
    /*cod_vec_init($$.vals);*/
  /*}*/
  : SYMBOL {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, eth_ast_ident($1));
  }
  | SYMBOL '=' Expr {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, $3);
  }
  | Record ',' SYMBOL {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, eth_ast_ident($3));
  }
  | Record ',' SYMBOL '=' Expr {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, $5);
  }
;

Dict
  : Expr ':' Expr {
    cod_vec_init($$.keys);
    cod_vec_init($$.vals);
    cod_vec_push($$.keys, $1);
    cod_vec_push($$.vals, $3);
  }
  | Dict ',' Expr ':' Expr {
    $$ = $1;
    cod_vec_push($$.keys, $3);
    cod_vec_push($$.vals, $5);
  }
;


/*RecordOrDict*/
  /*: SYMBOL {*/
    /*cod_vec_init($$.keys);*/
    /*cod_vec_init($$.vals);*/
    /*cod_vec_push($$.keys, $1);*/
    /*cod_vec_push($$.vals, eth_ast_ident($1));*/
  /*}*/
  /*: SYMBOL '=' Expr {*/
    /*cod_vec_init($$.keys);*/
    /*cod_vec_init($$.vals);*/
    /*cod_vec_push($$.keys, $1);*/
    /*cod_vec_push($$.vals, $3);*/
  /*}*/
  /*| Record ',' SYMBOL {*/
    /*$$ = $1;*/
    /*cod_vec_push($$.keys, $3);*/
    /*cod_vec_push($$.vals, eth_ast_ident($3));*/
  /*}*/
  /*| Record ',' SYMBOL '=' Expr {*/
    /*$$ = $1;*/
    /*cod_vec_push($$.keys, $3);*/
    /*cod_vec_push($$.vals, $5);*/
  /*}*/
/*;*/

Struct
  : {
    cod_vec_init($$.fldnams);
    cod_vec_init($$.fldvals);
    cod_vec_init($$.methods);
    cod_vec_init($$.methimpls);
    cod_vec_init($$.mutfields);
  }
  | Struct VAL SYMBOL '=' Expr {
    $$ = $1;
    cod_vec_push($$.fldnams, $3);
    cod_vec_push($$.fldvals, $5);
  }
  | Struct VAL MUT SYMBOL '=' Expr {
    $$ = $1;
    cod_vec_push($$.mutfields, $$.fldnams.len);
    cod_vec_push($$.fldnams, $4);
    cod_vec_push($$.fldvals, $6);
  }
  | Struct IMPL Atom ArgPatterns '=' Expr {
    $$ = $1;
    cod_vec_push($$.methods, $3);
    eth_ast *impl = eth_ast_fn_with_patterns($4.data, $4.len, $6);
    LOC(impl, @$);
    cod_vec_push($$.methimpls, impl);
    cod_vec_destroy($4);
  }
  | Struct IMPL Atom ArgPatterns {
    $$ = $1;
    cod_vec_push($$.methods, $3);
    cod_vec_push($$.methimpls, eth_ast_cval(eth_nil));
    cod_vec_destroy($4);
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

