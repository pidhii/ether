%{
#include <pcre.h>
#include "ether/ether.h"

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

extern int _eth_start_token;
static int g_comment_cnt = 0;

%}

%option noyywrap
%option reentrant
/*%option interactive*/

%x STRING
%x REGEXP
%x COMMENT

OPERATOR [-+%^.$:!<>*/=&|∘]+

%%

%{
  if (_eth_start_token >= 0)
  {
    int token = _eth_start_token;
    _eth_start_token = -1;
    return token;
  }
%}

[ \t\n] // white spaces

"(/)"     {
  yylval->string = strdup("/");
  return SYMBOL;
}

^"#!".*$
"--".*$

"--[[" { BEGIN(COMMENT); }
<COMMENT>"--[[" { g_comment_cnt += 1; }
<COMMENT>"--]]" {
  if (g_comment_cnt > 0)
    g_comment_cnt -= 1;
  else
    BEGIN(INITIAL);
}
<COMMENT>.
<COMMENT>\n

\" { BEGIN(STRING); return '"'; }
"\\" {
  BEGIN(REGEXP);
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
<REGEXP>"\\/"  { yylval->character = '/'; return CHAR; }
<STRING>\"     { BEGIN(INITIAL); return '"'; }
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
  BEGIN(INITIAL);
  yylval->integer = flag;
  return END_REGEXP;
}
<STRING,REGEXP>. { yylval->character = yytext[0]; return CHAR; }
<STRING>\n     { yylval->character = yytext[0]; return CHAR; }

nan                                 |
inf                                 |
0[xX][0-9a-fA-F]+                   |
[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)? {
  yylval->number = strtold(yytext, NULL);
  return NUMBER;
}

let    { return LET; }
rec    { return REC; }
and    { return AND; }
in     { return IN; }
if     { return IF; }
then   { return THEN; }
else   { return ELSE; }
when   { return WHEN; }
unless { return UNLESS; }
fn     { return FN; }
if[ \t\n]+let   { return IFLET; }
when[ \t\n]+let { return WHENLET; }
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

nil   { yylval->constant = eth_nil;   return CONST; }
true  { yylval->constant = eth_true;  return CONST; }
false { yylval->constant = eth_false; return CONST; }

pub       { return PUB; }
__builtin { return BUILTIN; }

"|"     |
"_"     |
"^"     |
"."     |
"$"     |
"!"     |
">"|"<" |
"+"|"-" |
"*"|"/" |
"("|")" |
"["|"]" |
"{"|"}" |
"="     |
","     |
";"     { return yytext[0]; }
"∘"     { return COMPOSE; }
"::"    { return CONS; }
"<="    { return LE; }
">="    { return GE; }
"=="    { return EQ; }
"/="    { return NE; }
"=~"    { return EQ_TILD; }
is      { return IS; }
is[ \v\t\n]+not { return ISNOT; }
eq      { return EQUAL; }
not[ \v\t\n]+eq { return NOTEQUAL; }
"->"    { return RARROW; }
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
"|>"    { return PIPE; }
"."\s*"(" { return DOT_OPEN1; }
"."\s*"[" { return DOT_OPEN2; }


"("(mod|land|lor|lxor|lshl|lshr|ashl|ashr|is|not|lnot)")" |
"("{OPERATOR}")"     {
  char *buf = malloc(yyleng - 2 + 1);
  strncpy(buf, yytext + 1, yyleng - 2);
  buf[yyleng - 2] = 0;
  yylval->string = buf;
  return SYMBOL;
}

{OPERATOR} {
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

. {
  eth_warning("undefined symbol '%c'", yytext[0]);
  return UNDEFINED;
}

%%
