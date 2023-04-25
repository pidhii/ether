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
#define fmtbracecnt (yyextra->fmtbracecnt)
#define quotestack (yyextra->quotestack)
#define push_quote(tag, str) _push_quote(yyextra, tag, str)
#define pop_quote() _pop_quote(yyextra)
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

OPCHR    [-+^.$:!<>*/=&|∘~%]
OPERATOR {OPCHR}+|mod|land|lor|lxor|lshl|lshr|ashl|ashr|is|eq|not|lnot

%%

%{
  if (statestack.len == 0)
    ENTER_STATE(INITIAL);
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

^"#!".*$
"--".*$
[[:space:]]

"|"|"|>" return PIPE;


let             { return LET; }
and             { return AND; }
if[ \t\n]+let   { return IFLET; }
fn              { return FN; }

"{" {
  if (fmtbracecnt.len > 0)
    cod_vec_last(fmtbracecnt) ++;
  return '{';
}

"}" {
  if (fmtbracecnt.len > 0 and cod_vec_last(fmtbracecnt)-- == 0)
  {
    cod_vec_pop(fmtbracecnt);
    PREV_STATE();
    return END_FORMAT;
  }
  return '}';
}


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
"{"[-a-zA-Z0-9_+]+"|" {
  char buf[yyleng-1];
  memcpy(buf, yytext+1, yyleng-2);
  buf[yyleng-2] = '\0';
  push_quote(QUOTES_STRING, buf);
  ENTER_STATE(STRING);
  return '"';
}
[[:space:]]+"/"/[^[:space:]] {
  ENTER_STATE(REGEXP);
  /*char *p = strchr(yytext, '/');*/
  /*yyless(p - yytext + 1);*/
  yylval->character = '/';
  return START_REGEXP;
}
<STRING,QSTRING>^[ \t]*\n { yylval->string = strdup(yytext); return STR; }
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
  cod_vec_push(fmtbracecnt, 0);
  ENTER_STATE(INITIAL);
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
if     { return IF; }
then   { return THEN; }
(";"[[:space:]]*)?else   { return ELSE; }
as     { return AS; }
try    { return TRY; }
with   { return WITH; }
or     { return OR; }
do     { return DO; }
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

"("({OPERATOR}|","+)")"     {
  char *buf = eth_malloc(yyleng - 2 + 1);
  strncpy(buf, yytext + 1, yyleng - 2);
  buf[yyleng - 2] = 0;
  yylval->string = buf;
  return SYMBOL;
}


"@"     |
"="     |
"_"     |
"^"     |
"."     |
":"     |
"#"     |
"$"     |
"!"     |
">"|"<" |
"+"|"-" |
"*"|"/" |
","     |
";"     { return yytext[0]; }
%       { return '%'; }
"->"    { return RARROW; }
"∘"     { return COMPOSE; }
"::"    { return CONS; }
"<="    { return LE; }
">="    { return GE; }
"=="    { return EQ; }
"!="    { return NE; }
"=~"    { return EQ_TILD; }
is      { return IS; }
is[ \t\n]+not { return ISNOT; }
is[[:space:]]+of { return ISOF; }
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
"..*"   { return DDDOTR; }
"*.."   { return DDDOTL; }
"..."   { return DDDOT; }
":="   { return ASSIGN; }


{OPCHR}+ {
  if (yytext[0] == '/')
    REJECT;
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

. {
  eth_warning("undefined symbol '%c', state %d", yytext[0], curstate);
  return UNDEFINED;
}

%%
