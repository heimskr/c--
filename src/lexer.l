%{
#include "Lexer.h"
#ifdef YY_USER_ACTION
#error "YY_USER_ACTION is already defined"
#endif
#define YY_USER_ACTION { cpmLexer.advance(cpmtext); }
#define RTOKEN(x) return cpmLexer.token(cpmtext, CPMTOK_##x);
%}

%option 8bit
%option debug
%option nobackup
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option warn

CPM_DECIMAL		([0-9]+([su](8|16|32|64))?)
CPM_HEX			(0x[0-9a-fA-F]+([su](8|16|32|64))?)
CPM_IDENT		([a-zA-Z_$`][`$a-zA-Z_0-9]*)
CPM_STRING		(\"([^\\"\n]|\\([\\"nratbe0]|x[0-9a-fA-F][0-9a-fA-F]))*\")
CPM_CHAR		(\'([^\\'\n]|\\([\\'nratbe0]|x[0-9a-fA-F][0-9a-fA-F]))\')
CPM_MLCOMMENT	("/*"([^*]|"*"[^/])*"*/")
CPM_SLCOMMENT	("//"[^\n]*)

%%

{CPM_MLCOMMENT}	{}
{CPM_SLCOMMENT}	{}
[ \t]+			{}
\n				{cpmLexer.newline();}

"#const"		{RTOKEN(CONSTATTR)}
"::"			{RTOKEN(SCOPE)}
"+="			{RTOKEN(PLUSEQ)}
"-="			{RTOKEN(MINUSEQ)}
"/="			{RTOKEN(DIVEQ)}
"*="			{RTOKEN(TIMESEQ)}
"%="			{RTOKEN(MODEQ)}
">>="			{RTOKEN(SREQ)}
"<<="			{RTOKEN(SLEQ)}
"&="			{RTOKEN(ANDEQ)}
"|="			{RTOKEN(OREQ)}
"^="			{RTOKEN(XOREQ)}
"++"			{RTOKEN(PLUSPLUS)}
"--"			{RTOKEN(MINUSMINUS)}
"->"			{RTOKEN(ARROW)}
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
"^^"			{RTOKEN(LXOR)}
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
"#name"			{RTOKEN(META_NAME)}
"#author"		{RTOKEN(META_AUTHOR)}
"#version"		{RTOKEN(META_VERSION)}
"#orcid"		{RTOKEN(META_ORCID)}
"#naked"		{RTOKEN(NAKED)}
"#saved"		{RTOKEN(SAVED)}
"#"				{RTOKEN(HASH)}
"return"		{RTOKEN(RETURN)}
"."				{RTOKEN(PERIOD)}
"~"				{RTOKEN(TILDE)}
"s8"			{RTOKEN(S8)}
"u8"			{RTOKEN(U8)}
"s16"			{RTOKEN(S16)}
"u16"			{RTOKEN(U16)}
"s32"			{RTOKEN(S32)}
"u32"			{RTOKEN(U32)}
"s64"			{RTOKEN(S64)}
"u64"			{RTOKEN(U64)}
"bool"			{RTOKEN(BOOL)}
"void"			{RTOKEN(VOID)}
"break"			{RTOKEN(BREAK)}
"continue"		{RTOKEN(CONTINUE)}
"for"			{RTOKEN(FOR)}
"%"				{RTOKEN(MOD)}
"asm"			{RTOKEN(ASM)}
"struct"		{RTOKEN(STRUCT)}
"sizeof"		{RTOKEN(SIZEOF)}
"null"			{RTOKEN(NULL)}
"const"			{RTOKEN(CONST)}
"offsetof"		{RTOKEN(OFFSETOF)}
"static"		{RTOKEN(STATIC)}
"new"			{RTOKEN(NEW)}
"delete"		{RTOKEN(DELETE)}
"operator"		{RTOKEN(OPERATOR)}

{CPM_CHAR}		{RTOKEN(CHAR)}
{CPM_STRING}	{RTOKEN(STRING)}
{CPM_DECIMAL}	{RTOKEN(NUMBER)}
{CPM_HEX}		{RTOKEN(NUMBER)}
{CPM_IDENT}		{RTOKEN(IDENT)}
.				{cpmLexer.badchar(*yytext);}

%%

#undef RTOKEN
