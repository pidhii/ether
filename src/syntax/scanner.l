%{
#include "ether/ether.h"

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

%}

%option noyywrap
%option reentrant
/*%option interactive*/

%x STRING

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
"#".*$ // comments

\" { BEGIN(STRING); return '"'; }
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
<STRING>"\\%"  { yylval->character = '%'; return CHAR; }
<STRING>"\\\"" { yylval->character = '"'; return CHAR; }
<STRING>\"     { BEGIN(INITIAL); return '"'; }
<STRING>.      { yylval->character = yytext[0]; return CHAR; }
<STRING>\n     { yylval->character = yytext[0]; return CHAR; }

nan |
[-+]?inf |
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
pub    { return PUB; }
import { return IMPORT; }
as     { return AS; }
unqualified { return UNQUALIFIED; }

nil   { yylval->constant = eth_nil;   return CONST; }
true  { yylval->constant = eth_true;  return CONST; }
false { yylval->constant = eth_false; return CONST; }

"."     |
"$"     |
":"     |
"!"     |
">"|"<" |
"+"|"-" |
"*"|"/" |
"("|")" |
"="     |
","     |
";"     { return yytext[0]; }
"<="    { return LE; }
">="    { return GE; }
"=="    { return EQ; }
"/="    { return NE; }
is      { return IS; }
"->"    { return RARROW; }

[_a-z][_a-zA-Z0-9]* {
  yylval->string = strdup(yytext);
  return SYMBOL;
}

[A-Z][_a-zA-Z0-9]* {
  yylval->string = strdup(yytext);
  return CAPSYMBOL;
}

%%
