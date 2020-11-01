%{
#include "ether/ether.h"

#include <pcre.h>
#include <ctype.h>
#include <unistd.h>


ETH_MODULE("ether:scanner");


#define YY_DECL \
  int yylex(YYSTYPE *yylval, YYLTYPE *yylloc, eth_scanner *yyscanner)

#define YY_USER_ACTION                          \
    yylloc->first_line = yylloc->last_line;     \
    yylloc->first_column = yylloc->last_column; \
    for(int i = 0; yytext[i] != '\0'; i++) {    \
        if(yytext[i] == '\n') {                 \
            yylloc->last_line++;                \
            yylloc->last_column = 1;            \
        }                                       \
        else {                                  \
            yylloc->last_column++;              \
        }                                       \
    }

#include "parser.h"

#include <string.h>

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

cod_vec(int) _eth_primary_tokens;
static int g_comment_cnt = 0;

enum { QUOTES_DEFAULT, QUOTES_STRING };
typedef struct { int tag; char *str; } quote;

static inline void
destroy_quote(quote q)
  {
    if (q.tag == QUOTES_STRING)
      free(q.str);
  }

static cod_vec(quote) g_quotes;

static inline void
push_quote(int tag, const char *maybestr)
  {
    quote q = { .tag = tag, .str = maybestr ? strdup(maybestr) : NULL };
    cod_vec_push(g_quotes, q);
  }

static inline void
pop_quote()
  {
    destroy_quote(cod_vec_pop(g_quotes));
  }

static cod_vec(int) g_fmtbracecnt;

struct indent_level { int nspaces; bool issilent; };
static cod_vec(struct indent_level) g_indent_stack;
static int g_cur_indent = 0;

static cod_vec(int) g_indentlvl_stack;

static cod_vec(int) g_state_stack;
static int g_cur_state = 0 /* INITIAL */;

#define ENTER_STATE(state)                    \
  do {                                        \
    /*eth_trace("ENTER '" #state "'");*/      \
    cod_vec_push(g_state_stack, g_cur_state); \
    BEGIN((g_cur_state = state));             \
  } while (0)

#define PREV_STATE()                                   \
  do {                                                 \
    /*eth_trace("POP STATE");*/                        \
    BEGIN((g_cur_state = cod_vec_pop(g_state_stack))); \
  } while (0)

/*#define CHANGE_STATE(state)                    \*/
  /*do {                                         \*/
    /*eth_trace("CHANGE STATE TO '" #state "'"); \*/
    /*g_cur_state = state;                       \*/
    /*BEGIN(state);                              \*/
  /*} while (0)*/

void
_eth_init_scanner(void)
  {
    cod_vec_init(_eth_primary_tokens);
    cod_vec_init(g_quotes);
    cod_vec_init(g_fmtbracecnt);
    cod_vec_init(g_indent_stack);
    cod_vec_init(g_state_stack);
    cod_vec_init(g_indentlvl_stack);
  }

void
_eth_cleanup_scanner(void)
  {
    cod_vec_destroy(_eth_primary_tokens);
    assert(g_quotes.len == 0);
    cod_vec_destroy(g_quotes);
    cod_vec_destroy(g_fmtbracecnt);
    cod_vec_destroy(g_indent_stack);
    cod_vec_destroy(g_state_stack);
    cod_vec_destroy(g_indentlvl_stack);
  }
%}

%option noyywrap
%option reentrant
%option interactive

%x QSTRING
%x STRING
%x REGEXP
%x COMMENT
%x INDENT
%s IGNORE_INDENT

OPCHR    [-+%^.$:!<>*/=&|∘~]
OPERATOR {OPCHR}+|mod|land|lor|lxor|lshl|lshr|ashl|ashr|is|eq|not|lnot

%%

%{
  if (_eth_primary_tokens.len > 0)
  {
    int token = cod_vec_pop(_eth_primary_tokens);
    if (token == START_REPL)
    {
      eth_debug("enable interactive mode");
      yy_set_interactive(1);
    }
    return token;
  }
%}

<INITIAL>^"#!".*$

<INDENT>^" "*\n

<INITIAL,INDENT,IGNORE_INDENT>[[:space:]]*"--".*$ {
  if (strncmp(yytext, "--[""[", 4) == 0)
    REJECT;
}

<INITIAL,INDENT,IGNORE_INDENT>[[:space:]]*"--[[" {
  ENTER_STATE(COMMENT);
}
<COMMENT>"--[[" { g_comment_cnt += 1; }
<COMMENT>"]]" {
  if (g_comment_cnt > 0)
    g_comment_cnt -= 1;
  else
    PREV_STATE();
}
<COMMENT>.
<COMMENT>\n

<INDENT>^" "*"|>" {
  /*
   * 1. Close all preceding blocks up to `|>`-indentation.
   * 2. Return to INITIAL-state.
   */
  cod_vec_push(_eth_primary_tokens, PIPE);

  int newindent = yyleng - 2;

  /* Drop preceding indent-levels */
  int diff = g_cur_indent - newindent;
  while (diff > 0)
  {
    assert(g_indent_stack.len > 0);
    struct indent_level indent = cod_vec_pop(g_indent_stack);
    if (not indent.issilent)
      cod_vec_push(_eth_primary_tokens, END_BLOCK);
    diff -= indent.nspaces;
  }
  if (diff < 0)
  {
    eth_error("malformed indentation");
    const char *path = filename(eth_get_scanner_input(yyscanner));
    if (path)
    {
      eth_location *loc = eth_create_location(yylloc->first_line,
          yylloc->first_column, yylloc->last_line, yylloc->last_column, path);
      eth_print_location(loc, stderr);
    }
    abort();
  }

  /* Create new indent level */
  /*struct indent_level indent;*/
  /*indent.nspaces = newindent - g_cur_indent;*/
  /*indent.issilent = true;*/
  /*cod_vec_push(g_indent_stack, indent);*/
  g_cur_indent = newindent;

  PREV_STATE();
  return cod_vec_pop(_eth_primary_tokens);
}
"|>" return PIPE;


<INDENT>^" "*/[^[:space:]] {
  PREV_STATE();

  if (yyleng < g_cur_indent)
  {
    /* Will yield KEEP_BLOCK right after the last END_BLOCK (as we are at the
     * moment at a line which is KEEP_BLOCKed to some block). */
    cod_vec_push(_eth_primary_tokens, KEEP_BLOCK);

    /* Drop indent-levels */
    int diff = g_cur_indent - yyleng;
    while (diff > 0)
    {
      assert(g_indent_stack.len > 0);
      struct indent_level indent = cod_vec_pop(g_indent_stack);
      if (not indent.issilent)
        cod_vec_push(_eth_primary_tokens, END_BLOCK);
      diff -= indent.nspaces;
    }
    if (diff < 0)
    {
      eth_error("malformed indentation");
      const char *path = filename(eth_get_scanner_input(yyscanner));
      if (path)
      {
        eth_location *loc = eth_create_location(yylloc->first_line,
            yylloc->first_column, yylloc->last_line, yylloc->last_column, path);
        eth_print_location(loc, stderr);
      }
      abort();
    }

    /* Update indentation */
    g_cur_indent = yyleng;

    /* ...we need something to return, so just pick a top element from the stack
     * (it will exsist since we allways push KEEP_BLOCK above) */
    int token = cod_vec_pop(_eth_primary_tokens);
    return token;
  }
  else if (yyleng > g_cur_indent)
  {
    /* Create new indent level */
    struct indent_level indent;
    indent.nspaces = yyleng - g_cur_indent;
    indent.issilent = false;
    cod_vec_push(g_indent_stack, indent);

    /* Update indentation */
    g_cur_indent = yyleng;

    return START_BLOCK;
  }
  else /* yyleng == g_cur_indent */
  {
    return KEEP_BLOCK;
  }
}

<STRING,QSTRING>^[ \t]*\n { yylval->string = strdup(yytext); return STR; }
<*>^[ \t]*\n


 /*****************************************************************************/
 /* Enter IGNORE_INDENT-state when expecting pattern, patterns do not care of
  * indent at all anyway. We need it here to propperly handle comas (','):
  * must prevent scanner from emitting END_BLOCKs when NOT parsing brace-enclo-
  * sed expressions.
  *
  * TODO: Handle WITH in similar fashion but ONLY when it is a part of TRY-WITH.
  * Porbably have to intoduce CATCH keyword to have TRY-CATCH (and then hande
  * the CATCH here).
  */
let             { ENTER_STATE(IGNORE_INDENT); return LET; }
and             { ENTER_STATE(IGNORE_INDENT); return AND; }
if[ \t\n]+let   { ENTER_STATE(IGNORE_INDENT); return IFLET; }
when[ \t\n]+let { ENTER_STATE(IGNORE_INDENT); return WHENLET; }
fn              { ENTER_STATE(IGNORE_INDENT); return FN; }

<IGNORE_INDENT>"="  { PREV_STATE(); return '='; }
<IGNORE_INDENT>"->" { PREV_STATE(); return RARROW; }

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
<IGNORE_INDENT>\n

<IGNORE_INDENT>"."[[:space:]]*"{" {
  if (g_fmtbracecnt.len > 0)
    cod_vec_last(g_fmtbracecnt) ++;
  ENTER_STATE(IGNORE_INDENT); /* Need it to properly handle patterns */
  return DOT_OPEN3;
}

<IGNORE_INDENT>"{" {
  if (g_fmtbracecnt.len > 0)
    cod_vec_last(g_fmtbracecnt) ++;
  ENTER_STATE(IGNORE_INDENT); /* Need it to properly handle patterns */
  return '{';
}

<IGNORE_INDENT>[[:space:]]*","[[:space:]]* { return ','; }

<IGNORE_INDENT>"}" {
  PREV_STATE();
  if (g_fmtbracecnt.len > 0 and cod_vec_last(g_fmtbracecnt)-- == 0)
  {
    cod_vec_pop(g_fmtbracecnt);
    return END_FORMAT;
  }
  return '}';
}

<IGNORE_INDENT>"("|")" |
<IGNORE_INDENT>"["|"]" { return yytext[0]; }
 /*****************************************************************************/



 /*****************************************************************************/
("."[[:space:]]*)?[[({][[:space:]]* {
  /* Save number of indent levels before entering braces. */
  cod_vec_push(g_indentlvl_stack, g_indent_stack.len);

  /* If there is a line-break after the brace, set indentation w.r.t. the RHS;
   * otherwize, keep current indentation. */
  char *p;
  if ((p = strrchr(yytext, '\n')))
  {
    int newindent = strlen(p + 1);

    /* Create new indent-level. */
    struct indent_level indent;
    indent.issilent = true;
    indent.nspaces = newindent - g_cur_indent;
    cod_vec_push(g_indent_stack, indent);
    g_cur_indent = newindent;
  }

  /* Start braces. */
  int brace;
  for (p = yytext + yyleng - 1; isspace(*p); --p);
  if (yytext[0] == '.')
  {
    switch (*p)
    {
      case '(': return DOT_OPEN1;
      case '[': return DOT_OPEN2;
      case '{': return DOT_OPEN3;
    }
  }
  return *p;
}

[[:space:]]*","[[:space:]]* {
  cod_vec_push(_eth_primary_tokens, ',');

  /* Drop all gained indentation levels. */
  int nindwas = cod_vec_last(g_indentlvl_stack);
  int nindnow = g_indent_stack.len;
  int n = nindnow - nindwas;
  while (n--)
  {
    struct indent_level indent = cod_vec_pop(g_indent_stack);
    if (not indent.issilent)
      cod_vec_push(_eth_primary_tokens, END_BLOCK);
    g_cur_indent -= indent.nspaces;
  }

  /* If there is a line-break between RHS and LHS of a coma, set indentation
   * w.r.t. the RHS; otherwize, keep current indentation. */
  char *p;
  if ((p = strrchr(yytext, '\n')))
  {
    int newindent = strlen(p + 1);

    /* Create new indent-level. */
    struct indent_level indent;
    indent.issilent = true;
    indent.nspaces = newindent - g_cur_indent;
    cod_vec_push(g_indent_stack, indent);
    g_cur_indent = newindent;
  }

  return cod_vec_pop(_eth_primary_tokens);
}

[[:space:]]*[\])}] {
  cod_vec_push(_eth_primary_tokens, yytext[yyleng-1]);

  /* Drop all gained indentation levels. */
  int nindwas = cod_vec_pop(g_indentlvl_stack);
  int nindnow = g_indent_stack.len;
  int n = nindnow - nindwas;
  while (n--)
  {
    struct indent_level indent = cod_vec_pop(g_indent_stack);
    if (not indent.issilent)
      cod_vec_push(_eth_primary_tokens, END_BLOCK);
    g_cur_indent -= indent.nspaces;
  }

  return cod_vec_pop(_eth_primary_tokens);
}
 /*****************************************************************************/


\n {
  ENTER_STATE(INDENT);
}

[ \t] // white spaces

  /*"(/)"     {*/
    /*yylval->string = strdup("/");*/
    /*return SYMBOL;*/
  /*}*/


\" {
  push_quote(QUOTES_DEFAULT, NULL);
  ENTER_STATE(STRING);
  return '"';
}
\' {
  push_quote(QUOTES_DEFAULT, NULL);
  ENTER_STATE(QSTRING);
  return '"';
}
"{.help|" {
  push_quote(QUOTES_STRING, ".help");
  ENTER_STATE(QSTRING);
  return HELP;
}
"{"[-a-zA-Z0-9_+]+"|" {
  char buf[yyleng-1];
  memcpy(buf, yytext+1, yyleng-2);
  buf[yyleng-2] = '\0';
  push_quote(QUOTES_STRING, buf);
  ENTER_STATE(STRING);
  return '"';
}
"\\" {
  ENTER_STATE(REGEXP);
  yylval->character = yytext[yyleng-1];
  return START_REGEXP;
}
<STRING>\\\n
<STRING>"\\0"  { yylval->character = '\0'; return CHAR; }
<STRING>"\\a"  { yylval->character = '\a'; return CHAR; }
<STRING>"\\b"  { yylval->character = '\b'; return CHAR; }
<STRING>"\\e"  { yylval->character = '\x1b'; return CHAR; }
<STRING>"\\f"  { yylval->character = '\f'; return CHAR; }
<STRING>"\\n"  { yylval->character = '\n'; return CHAR; }
<STRING>"\\r"  { yylval->character = '\r'; return CHAR; }
<STRING>"\\t"  { yylval->character = '\t'; return CHAR; }
<STRING>"\\v"  { yylval->character = '\v'; return CHAR; }
<STRING>"\\?"  { yylval->character = '\?'; return CHAR; }
<STRING>"\\x"[0-9a-fA-F]{1,2} {
  char *endptr;
  yylval->character = strtol(yytext + 2, &endptr, 16);
  assert(*endptr == '\0');
  return CHAR;
}
<STRING>"\\\"" { yylval->character = '"'; return CHAR; }
<QSTRING>"\\\'" { yylval->character = '\''; return CHAR; }
<STRING>"\\\\" { yylval->character = '\\'; return CHAR; }
<QSTRING,REGEXP>"\\/"  { yylval->character = '/'; return CHAR; }

<STRING>"\\{" { yylval->character = '{'; return CHAR; }
<STRING>"{" {
  cod_vec_push(g_fmtbracecnt, 0);
  ENTER_STATE(IGNORE_INDENT);
  return START_FORMAT;
}

<STRING>\" {
  if (cod_vec_last(g_quotes).tag == QUOTES_DEFAULT)
  {
    pop_quote();
    PREV_STATE();
    return '"';
  }
  else
  {
    yylval->character = '"';
    return CHAR;
  }
}
<QSTRING>\' {
  if (cod_vec_last(g_quotes).tag == QUOTES_DEFAULT)
  {
    pop_quote();
    PREV_STATE();
    return '"';
  }
  else
  {
    yylval->character = '"';
    return CHAR;
  }
}
<QSTRING>"|.help}" {
  if (strcmp(".help", cod_vec_last(g_quotes).str) == 0)
  {
    pop_quote();
    PREV_STATE();
    return HELP;
  }
  else
  {
    yylval->string = strdup(".help");
    return STR;
  }
}
<STRING>"|"[-a-zA-Z0-9_+]+"}" {
  char buf[yyleng-1];
  memcpy(buf, yytext+1, yyleng-2);
  buf[yyleng-2] = '\0';
  if (strcmp(buf, cod_vec_last(g_quotes).str) == 0)
  {
    pop_quote();
    PREV_STATE();
    return '"';
  }
  else
  {
    yylval->string = strdup(buf);
    return STR;
  }
}
<REGEXP>"/"[a-zA-Z]* {
  int flag = 0;
  for (char *p = yytext + 1; *p; ++p)
  {
    switch (*p)
    {
      case 'i': flag |= PCRE_CASELESS ; break;
      case 'm': flag |= PCRE_MULTILINE; break;
      case 's': flag |= PCRE_DOTALL   ; break;
      case 'x': flag |= PCRE_EXTENDED ; break;

    }
  }
  PREV_STATE();
  yylval->integer = flag;
  return END_REGEXP;
}
<QSTRING,STRING,REGEXP>. { yylval->character = yytext[0]; return CHAR; }
<QSTRING,STRING>\n     { yylval->character = yytext[0]; return CHAR; }

nan                                 |
inf                                 |
0[xX][0-9a-fA-F][0-9a-fA-F_]*       |
[0-9][0-9_]*(\.[0-9][0-9_]*)?([eE][-+]?[0-9][0-9_]*)? {
  char buf[yyleng+1];
  int buflen = 0;
  for (char *p = yytext; *p; ++p)
  {
    if (*p != '_')
      buf[buflen++] = *p;
  }
  buf[buflen] = '\0';
  yylval->number = strtold(buf, NULL);
  return NUMBER;
}


rec    { return REC; }
in     { return IN; }
if     { return IF; }
then   { return THEN; }
else   { return ELSE; }
when   { return WHEN; }
unless { return UNLESS; }
open   { return OPEN; }
using  { return USING; }
as     { return AS; }
try    { return TRY; }
with   { return WITH; }
or     { return OR; }
begin  { return BEGINN; }
end    { return END; }
do     { return DO; }
done   { return DONE; }
case   { return CASE; }
of     { return OF; }
module { return MODULE; }
lazy   { return LAZY; }
assert { return ASSERT; }

nil   { yylval->constant = eth_nil;   return CONST; }
true  { yylval->constant = eth_true;  return CONST; }
false { yylval->constant = eth_false; return CONST; }

pub          { return PUB; }
__builtin    { return BUILTIN; }
__deprecated { return DEPRECATED; }

"("|")" |
"["|"]" { return yytext[0]; }
"."[[:space:]]*"(" { return DOT_OPEN1; }
"."[[:space:]]*"[" { return DOT_OPEN2; }

"="     |
"|"     |
"_"     |
"^"     |
"."     |
":"     |
"$"     |
"!"     |
">"|"<" |
"+"|"-" |
"*"|"/" |
","     |
";"     { return yytext[0]; }
"->"    { return RARROW; }
"∘"     { return COMPOSE; }
"::"    { return CONS; }
"<="    { return LE; }
">="    { return GE; }
"=="    { return EQ; }
"/="    { return NE; }
"=~"    { return EQ_TILD; }
is      { return IS; }
is[ \t\n]+not { return ISNOT; }
eq      { return EQUAL; }
not[ \t\n]+eq { return NOTEQUAL; }
"<-"    { return LARROW; }
"&&"    { return OPAND; }
"||"    { return OPOR; }
mod     { return MOD; }
land    { return LAND; }
lor     { return LOR; }
lxor    { return LXOR; }
lshl    { return LSHL; }
lshr    { return LSHR; }
ashl    { return ASHL; }
ashr    { return ASHR; }
not     { return NOT; }
lnot    { return LNOT; }
"++"    { return PPLUS; }
".."    { return DDOT; }
"..."   { return DDDOT; }

"("{OPERATOR}")"     {
  char *buf = malloc(yyleng - 2 + 1);
  strncpy(buf, yytext + 1, yyleng - 2);
  buf[yyleng - 2] = 0;
  yylval->string = buf;
  return SYMBOL;
}

{OPCHR}+ {
  yylval->string = strdup(yytext);
  return USROP;
}

[_a-z][_a-zA-Z0-9]*['?]? {
  yylval->string = strdup(yytext);
  return SYMBOL;
}

[A-Z][_a-zA-Z0-9]* {
  yylval->string = strdup(yytext);
  return CAPSYMBOL;
}

<<EOF>> {
  cod_vec_push(_eth_primary_tokens, EOF);
  while (g_cur_indent > 0)
  {
    struct indent_level indent = cod_vec_pop(g_indent_stack);
    if (not indent.issilent)
      cod_vec_push(_eth_primary_tokens, END_BLOCK);
    g_cur_indent -= indent.nspaces;
  }
  if (g_cur_indent < 0)
  {
    eth_error("malformed indentation");
    abort();
  }
  assert(g_indent_stack.len == 0);

  /* Clean up state stack */
  while (g_state_stack.len > 0)
    g_cur_state = cod_vec_pop(g_state_stack);
  assert(g_cur_state == INITIAL);

  int token = cod_vec_pop(_eth_primary_tokens);
  return token;
}

. {
  eth_warning("undefined symbol '%c', state %d", yytext[0], g_cur_state);
  return UNDEFINED;
}

%%
