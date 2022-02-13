%{
#include "Lexer.h"
#ifdef YY_USER_ACTION
#error "YY_USER_ACTION is already defined"
#endif
#define YY_USER_ACTION { cmmLexer.advance(cmmtext); }
#define RTOKEN(x) return cmmLexer.token(cmmtext, CMMTOK_##x);
%}

%option 8bit
%option debug
%option nobackup
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option warn

CMM_DECIMAL		([0-9]+)
CMM_IDENT		([a-zA-Z_][a-zA-Z_0-9]*)

%%

[ \t]+			{}
\n				{cmmLexer.newline();}

"("				{RTOKEN(LPAREN)}
")"				{RTOKEN(RPAREN)}
"+"				{RTOKEN(PLUS)}
"-"				{RTOKEN(MINUS)}
"*"				{RTOKEN(TIMES)}
"/"				{RTOKEN(DIV)}
"^"				{RTOKEN(XOR)}
";"				{RTOKEN(SEMI)}
"&&"			{RTOKEN(LAND)}
"||"			{RTOKEN(LOR)}
"&"				{RTOKEN(AND)}
"|"				{RTOKEN(OR)}
"!"				{RTOKEN(NOT)}
"<<"			{RTOKEN(LSHIFT)}
">>"			{RTOKEN(RSHIFT)}
"<="			{RTOKEN(LTE)}
">="			{RTOKEN(GTE)}
"<"				{RTOKEN(LT)}
">"				{RTOKEN(GT)}
"!="			{RTOKEN(NEQ)}
"=="			{RTOKEN(DEQ)}
"="				{RTOKEN(ASSIGN)}
"true"			{RTOKEN(TRUE)}
"false"			{RTOKEN(FALSE)}
"if"			{RTOKEN(IF)}
"else"			{RTOKEN(ELSE)}
"while"			{RTOKEN(WHILE)}
"{"				{RTOKEN(LBRACE)}
"}"				{RTOKEN(RBRACE)}
"["				{RTOKEN(LSQUARE)}
"]"				{RTOKEN(RSQUARE)}
","				{RTOKEN(COMMA)}
"?"				{RTOKEN(QUESTION)}
":"				{RTOKEN(COLON)}
"#"				{RTOKEN(HASH)}
"fn"			{RTOKEN(FN)}
"return"		{RTOKEN(RETURN)}
"."				{RTOKEN(PERIOD)}
"i8"			{RTOKEN(S8)}
"u8"			{RTOKEN(U8)}
"i16"			{RTOKEN(S16)}
"u16"			{RTOKEN(U16)}
"i32"			{RTOKEN(S32)}
"u32"			{RTOKEN(U32)}
"i64"			{RTOKEN(S64)}
"u64"			{RTOKEN(U64)}
"bool"			{RTOKEN(BOOL)}
"void"			{RTOKEN(VOID)}


{CMM_DECIMAL}	{RTOKEN(NUMBER)}
{CMM_IDENT}		{RTOKEN(IDENT)}
.				{cmmLexer.badchar(*yytext);}

%%

#undef RTOKEN
