%{
#include "ether/ether.h"

#include "scanner-data.h"

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

#define YY_EXTRA_TYPE eth_scanner_data*
#define primtoks (yyextra->primtoks)
#define commentcnt (yyextra->commentcnt)
#define fmtbracecnt (yyextra->fmtbracecnt)
#define quotestack (yyextra->quotestack)
#define push_quote(tag, str) _push_quote(yyextra, tag, str)
#define pop_quote() _pop_quote(yyextra)
#define indentstack (yyextra->indentstack)
#define curindent (yyextra->curindent)
#define indlvlstack (yyextra->indlvlstack)
#define statestack (yyextra->statestack)
#define curstate (yyextra->curstate)

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

#define ENTER_STATE(state)               \
  do {                                   \
    /*eth_trace("ENTER '" #state "'");*/ \
    cod_vec_push(statestack, curstate);  \
    BEGIN((curstate = state));           \
  } while (0)

#define PREV_STATE()                             \
  do {                                           \
    /*eth_trace("POP STATE");*/                  \
    BEGIN((curstate = cod_vec_pop(statestack))); \
  } while (0)

/*#define CHANGE_STATE(state)                    \*/
  /*do {                                         \*/
    /*eth_trace("CHANGE STATE TO '" #state "'"); \*/
    /*curstate = state;                       \*/
    /*BEGIN(state);                              \*/
  /*} while (0)*/
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
  if (primtoks.len > 0)
  {
    int token = cod_vec_pop(primtoks);
    if (token == START_REPL)
    {
      eth_debug("enable interactive mode");
      assert(yyextra->isrepl);
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

<INITIAL,INDENT,IGNORE_INDENT>"--[""[" {
  ENTER_STATE(COMMENT);
}
<INITIAL,INDENT,IGNORE_INDENT>[[:space:]]*"--[[" {
  ENTER_STATE(COMMENT);
}
<COMMENT>"--[[" { commentcnt += 1; }
<COMMENT>"]]" {
  if (commentcnt > 0)
    commentcnt -= 1;
  else
    PREV_STATE();
}
<COMMENT>.
<COMMENT>\n

<INDENT>^" "*("|>"|"|") {
  /*
   * 1. Close all preceding blocks up to `|>`-indentation.
   * 2. Return to INITIAL-state.
   */
  cod_vec_push(primtoks, PIPE);

  int newindent = yyleng - (yytext[yyleng-1] == '>' ? 2 : 1);

  /* Drop preceding indent-levels */
  int diff = curindent - newindent;
  while (diff > 0)
  {
    assert(indentstack.len > 0);
    indent_level indent = cod_vec_pop(indentstack);
    if (not indent.issilent)
      cod_vec_push(primtoks, END_BLOCK);
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
  /*indent_level indent;*/
  /*indent.nspaces = newindent - curindent;*/
  /*indent.issilent = true;*/
  /*cod_vec_push(indentstack, indent);*/
  curindent = newindent;

  PREV_STATE();
  return cod_vec_pop(primtoks);
}
"|"|"|>" return PIPE;


<INDENT>^" "*/[^[:space:]] {
  PREV_STATE();

  if ((int)yyleng < curindent)
  {
    /* Will yield KEEP_BLOCK right after the last END_BLOCK (as we are at the
     * moment at a line which is KEEP_BLOCKed to some block). */
    cod_vec_push(primtoks, KEEP_BLOCK);

    /* Drop indent-levels */
    int diff = curindent - yyleng;
    while (diff > 0)
    {
      assert(indentstack.len > 0);
      indent_level indent = cod_vec_pop(indentstack);
      if (not indent.issilent)
        cod_vec_push(primtoks, END_BLOCK);
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
    curindent = yyleng;

    /* ...we need something to return, so just pick a top element from the stack
     * (it will exsist since we allways push KEEP_BLOCK above) */
    int token = cod_vec_pop(primtoks);
    return token;
  }
  else if ((int)yyleng > curindent)
  {
    /* Create new indent level */
    indent_level indent;
    indent.nspaces = yyleng - curindent;
    indent.issilent = false;
    cod_vec_push(indentstack, indent);

    /* Update indentation */
    curindent = yyleng;

    return START_BLOCK;
  }
  else /* yyleng == curindent */
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
class           { ENTER_STATE(IGNORE_INDENT); return CLASS; }
method          { ENTER_STATE(IGNORE_INDENT); return METHOD; }
let             { ENTER_STATE(IGNORE_INDENT); return LET; }
and             { ENTER_STATE(IGNORE_INDENT); return AND; }
if[ \t\n]+let   { ENTER_STATE(IGNORE_INDENT); return IFLET; }
when[ \t\n]+let { ENTER_STATE(IGNORE_INDENT); return WHENLET; }
fn              { ENTER_STATE(IGNORE_INDENT); return FN; }

<IGNORE_INDENT>"="  {
  if (cod_vec_last(statestack) != IGNORE_INDENT)
    PREV_STATE();
  return '=';
}
<IGNORE_INDENT>"->" { PREV_STATE(); return RARROW; }

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
<IGNORE_INDENT>\n

<IGNORE_INDENT>"."[[:space:]]*"{"[[:space:]]* {
  if (fmtbracecnt.len > 0)
    cod_vec_last(fmtbracecnt) ++;
  ENTER_STATE(IGNORE_INDENT); /* Need it to properly handle patterns */
  return DOT_OPEN3;
}

<IGNORE_INDENT>"{"[[:space:]]* {
  if (fmtbracecnt.len > 0)
    cod_vec_last(fmtbracecnt) ++;
  ENTER_STATE(IGNORE_INDENT); /* Need it to properly handle patterns */
  return '{';
}

<IGNORE_INDENT>[[:space:]]*","[[:space:]]* { return ','; }

<IGNORE_INDENT>[[:space:]]*"}" {
  PREV_STATE();
  if (fmtbracecnt.len > 0 and cod_vec_last(fmtbracecnt)-- == 0)
  {
    cod_vec_pop(fmtbracecnt);
    return END_FORMAT;
  }
  return '}';
}

<IGNORE_INDENT>"("|")" |
<IGNORE_INDENT>"["|"]" { return yytext[0]; }
 /*****************************************************************************/



 /*****************************************************************************/
("."([[:space:]]*"--".*\n)*[[:space:]]*)?[[({]([[:space:]]*"--".*\n)*[[:space:]]* {
  /* Remove comments.*/
  char text[yyleng+1];
  int leng = 0;
  for (char *p = yytext; *p; ++p)
  {
    if (yyleng - (p - yytext) > 1 and strncmp(p, "--", 2) == 0)
      while (*++p != '\n');
    else
      text[leng++] = *p;
  }
  text[leng] = '\0';

  /* Save number of indent levels before entering braces. */
  cod_vec_push(indlvlstack, indentstack.len);

  /* If there is a line-break after the brace, set indentation w.r.t. the RHS;
   * otherwize, keep current indentation. */
  char *p;
  if ((p = strrchr(text, '\n')))
  {
    int newindent = strlen(p + 1);

    /* Create new indent-level. */
    indent_level indent;
    indent.issilent = true;
    indent.nspaces = newindent - curindent;
    cod_vec_push(indentstack, indent);
    curindent = newindent;
  }

  /* Start braces. */
  int brace;
  for (p = text + leng - 1; isspace(*p); --p);
  if (text[0] == '.')
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

[[:space:]]*([[:space:]]*"--".*\n)*","([[:space:]]*"--".*\n)*[[:space:]]* {
  /* Remove comments.*/
  char text[yyleng+1];
  int leng = 0;
  for (char *p = yytext; *p; ++p)
  {
    if (yyleng - (p - yytext) > 1 and strncmp(p, "--", 2) == 0)
      while (*++p != '\n');
    text[leng++] = *p;
  }
  text[leng] = '\0';

  cod_vec_push(primtoks, ',');
  /* Drop all gained indentation levels. */
  assert(indlvlstack.len > 0);
  int nindwas = cod_vec_last(indlvlstack);
  int nindnow = indentstack.len;
  int n = nindnow - nindwas;
  while (n--)
  {
    assert(indentstack.len > 0);
    indent_level indent = cod_vec_pop(indentstack);
    if (not indent.issilent)
      cod_vec_push(primtoks, END_BLOCK);
    curindent -= indent.nspaces;
  }

  /* If there is a line-break between RHS and LHS of a coma, set indentation
   * w.r.t. the RHS; otherwize, keep current indentation. */
  char *p;
  if ((p = strrchr(text, '\n')))
  {
    int newindent = strlen(p + 1);

    /* Create new indent-level. */
    indent_level indent;
    indent.issilent = true;
    indent.nspaces = newindent - curindent;
    cod_vec_push(indentstack, indent);
    curindent = newindent;
  }

  return cod_vec_pop(primtoks);
}

([[:space:]]*"--".*\n)*[[:space:]]*[\])}] {
  cod_vec_push(primtoks, yytext[yyleng-1]);

  /* Drop all gained indentation levels. */
  if (indlvlstack.len <= 0)
  {
    eth_error("indentation failure");
    const char *path = filename(eth_get_scanner_input(yyscanner));
    if (path)
    {
      eth_location *loc = eth_create_location(yylloc->first_line,
          yylloc->first_column, yylloc->last_line, yylloc->last_column, path);
      eth_print_location(loc, stderr);
    }
    abort();
  }
  assert(indlvlstack.len > 0);
  int nindwas = cod_vec_pop(indlvlstack);
  int nindnow = indentstack.len;
  int n = nindnow - nindwas;
  while (n--)
  {
    assert(indentstack.len > 0);
    indent_level indent = cod_vec_pop(indentstack);
    if (not indent.issilent)
      cod_vec_push(primtoks, END_BLOCK);
    curindent -= indent.nspaces;
  }

  return cod_vec_pop(primtoks);
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
<STRING>"{"[[:space:]]* {
  cod_vec_push(fmtbracecnt, 0);
  ENTER_STATE(IGNORE_INDENT);
  return START_FORMAT;
}

<STRING>\" {
  if (cod_vec_last(quotestack).tag == QUOTES_DEFAULT)
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
  if (cod_vec_last(quotestack).tag == QUOTES_DEFAULT)
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
  if (strcmp(".help", cod_vec_last(quotestack).str) == 0)
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
  if (strcmp(buf, cod_vec_last(quotestack).str) == 0)
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
0[xX][0-9a-fA-F][0-9a-fA-F_]*(\.[0-9a-fA-F]+[pP][-+]?[0-9a-fA-F]+)? |
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


open   { return OPEN; }
import { return IMPORT; }
rec    { return REC; }
in     { return IN; }
if     { return IF; }
then   { return THEN; }
else   { return ELSE; }
when   { return WHEN; }
object { return OBJECT; }
inherit { return INHERIT; }
unless { return UNLESS; }
as     { return AS; }
try    { return TRY; }
with   { return WITH; }
or     { return OR; }
do     { return DO; }
case   { return CASE; }
of     { return OF; }
lazy   { return LAZY; }
assert { return ASSERT; }
return { return RETURN; }

defined { return DEFINED; }

nil   { yylval->constant = eth_nil;   return CONST; }
true  { yylval->constant = eth_true;  return CONST; }
false { yylval->constant = eth_false; return CONST; }

pub          { return PUB; }
mut          { return MUT; }
__builtin    { return BUILTIN; }
__deprecated { return DEPRECATED; }

"("|")" |
"["|"]" { return yytext[0]; }
"."[[:space:]]*"(" { return DOT_OPEN1; }
"."[[:space:]]*"[" { return DOT_OPEN2; }

"@"     |
"="     |
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
":="   { return ASSIGN; }

"("({OPERATOR}|","+)")"     {
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

[_a-zA-Z][_a-zA-Z0-9]*['?]? {
  yylval->string = strdup(yytext);
  return SYMBOL;
}

`[_a-zA-Z0-9]* {
  yylval->string = strdup(yytext+1);
  return CAPSYMBOL;
}

<<EOF>> {
  /* TODO: don't clean up here (due to REPL) */
  cod_vec_push(primtoks, EOF);
  while (curindent > 0)
  {
    indent_level indent = cod_vec_pop(indentstack);
    if (not indent.issilent)
      cod_vec_push(primtoks, END_BLOCK);
    curindent -= indent.nspaces;
  }
  if (curindent < 0)
  {
    eth_error("malformed indentation");
    abort();
  }
  assert(indentstack.len == 0);

  /* Clean up state stack */
  while (statestack.len > 0)
    curstate = cod_vec_pop(statestack);
  assert(curstate == INITIAL);

  int token = cod_vec_pop(primtoks);
  return token;
}

. {
  eth_warning("undefined symbol '%c', state %d", yytext[0], curstate);
  return UNDEFINED;
}

%%
