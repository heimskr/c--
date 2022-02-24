%{
#include <cassert>
#include <cstdarg>
#include <initializer_list>

#define NO_YYPARSE
#include "Lexer.h"
#include "ASTNode.h"
#include "Parser.h"

template <typename ...Args>
void D(Args && ...args) {
	(void) std::initializer_list<int> {
		((void) delete args, 0)...
	};
}

using AN = ASTNode;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
%}

%debug
%defines
%define parse.error verbose
%token-table
%verbose

%define api.prefix {cmm}

%initial-action {
    cmmParser.root = new ASTNode(cmmParser, CMMTOK_ROOT, {0, 0}, "");
}

%token CMMTOK_ROOT CMMTOK_NUMBER CMMTOK_IDENT CMMTOK_STRING CMMTOK_CHAR
%token CMMTOK_LPAREN "("
%token CMMTOK_RPAREN ")"
%token CMMTOK_PLUS "+"
%token CMMTOK_MINUS "-"
%token CMMTOK_TIMES "*"
%token CMMTOK_DIV "/"
%token CMMTOK_XOR "^"
%token CMMTOK_TRUE "true"
%token CMMTOK_FALSE "false"
%token CMMTOK_IF "if"
%token CMMTOK_ELSE "else"
%token CMMTOK_WHILE "while"
%token CMMTOK_SEMI ";"
%token CMMTOK_DEQ "=="
%token CMMTOK_ASSIGN "="
%token CMMTOK_LAND "&&"
%token CMMTOK_LOR "||"
%token CMMTOK_AND "&"
%token CMMTOK_OR "|"
%token CMMTOK_LTE "<="
%token CMMTOK_GTE ">="
%token CMMTOK_LT "<"
%token CMMTOK_GT ">"
%token CMMTOK_NOT "!"
%token CMMTOK_TILDE "~"
%token CMMTOK_NEQ "!="
%token CMMTOK_LBRACE "{"
%token CMMTOK_RBRACE "}"
%token CMMTOK_LSQUARE "["
%token CMMTOK_RSQUARE "]"
%token CMMTOK_COMMA ","
%token CMMTOK_QUESTION "?"
%token CMMTOK_COLON ":"
%token CMMTOK_HASH "#"
%token CMMTOK_RETURN "return"
%token CMMTOK_PERIOD "."
%token CMMTOK_S8 "s8"
%token CMMTOK_U8 "u8"
%token CMMTOK_S16 "s16"
%token CMMTOK_U16 "u16"
%token CMMTOK_S32 "s32"
%token CMMTOK_U32 "u32"
%token CMMTOK_S64 "s64"
%token CMMTOK_U64 "u64"
%token CMMTOK_BOOL "bool"
%token CMMTOK_VOID "void"
%token CMMTOK_LSHIFT "<<"
%token CMMTOK_RSHIFT ">>"
%token CMMTOK_META_NAME "#name"
%token CMMTOK_META_AUTHOR "#author"
%token CMMTOK_META_VERSION "#version"
%token CMMTOK_META_ORCID "#orcid"
%token CMMTOK_BREAK "break"
%token CMMTOK_CONTINUE "continue"
%token CMMTOK_FOR "for"
%token CMMTOK_MOD "%"
%token CMMTOK_PLUSEQ "+="
%token CMMTOK_MINUSEQ "-="
%token CMMTOK_PLUSPLUS "++"
%token CMMTOK_MINUSMINUS "--"
%token CMMTOK_ASM "asm"
%token CMMTOK_NAKED "#naked"
%token CMMTOK_STRUCT "struct"
%token CMMTOK_ARROW "->"
%token CMMTOK_SIZEOF "sizeof"
%token CMMTOK_NULL "null"

%token CMM_LIST CMM_ACCESS CMM_BLOCK CMM_CAST CMM_ADDROF CMM_EMPTY CMM_POSTPLUS CMM_POSTMINUS CMM_FNPTR CMM_DECL

%start start

%left ";"
%right "?" "=" "+=" "-="
%left "||"
%left "&&"
%left "|"
%left "^"
%left BITWISE_AND
%left "==" "!="
%left "<" "<=" ">" ">="
%left "<<" ">>"
%left "+" "-"
%left MULT "/" "%"
%right "!" "~" "#" "sizeof" DEREF ADDROF PREFIX UNARY CAST
%left "[" POSTFIX CALL "." "->"
%nonassoc "else"

%%

start: program;

program: program decl_or_def ";" { $$ = $1->adopt($2); D($3); }
       | program function_def { $$ = $1->adopt($2); }
       | program meta { $$ = $1->adopt($2); }
       | program forward_decl { $$ = $1->adopt($2); }
       | program struct_def { $$ = $1->adopt($2); }
       | { $$ = cmmParser.root; };

meta_start: "#name" | "#author" | "#orcid" | "#version";

meta: meta_start string { $$ = $1->adopt($2); };

statement: block
         | conditional
         | while_loop
         | for_loop
         | decl_or_def ";" { D($2); }
         | expr ";" { D($2); }
         | "return" expr ";" { $$ = $1->adopt($2); D($3); }
         | "return" ";" { D($2); }
         | "break" ";" { D($2); }
         | "continue" ";" { D($2); }
         | ";" { $1->symbol = CMM_EMPTY; }
         | inline_asm;

inline_asm: "asm" "(" string ":" _exprlist ":" _exprlist ")" { $$ = $1->adopt({$3, $5, $7}); D($2, $4, $6, $8); }
          | "asm" "(" string ":" _exprlist ")" { $$ = $1->adopt({$3, $5}); D($2, $4, $6); }
          | "asm" "(" string ")" { $$ = $1->adopt({$3}); D($2, $4); };

declaration: type ident { $$ = (new ASTNode(cmmParser, CMM_DECL, $1->location))->adopt({$1, $2}); }
definition:  type ident "=" expr { $$ = (new ASTNode(cmmParser, CMM_DECL, $1->location))->adopt({$1, $2, $4}); D($3); }
decl_or_def: declaration | definition;

forward_decl: "struct" CMMTOK_IDENT ";" { $$ = $1->adopt($2); D($3); };

struct_def: "struct" CMMTOK_IDENT "{" decl_list "}" ";" { $$ = $1->adopt({$2, $4}); D($3, $5, $6); };

decl_list: decl_list type CMMTOK_IDENT ";" { $$ = $1->adopt($3->adopt($2)); D($4); }
         | { $$ = new ASTNode(cmmParser, CMM_LIST); };

function_def: type ident "(" _arglist ")" fnattrs block { $$ = $2->adopt({$1, $4, $6, $7}); D($3, $5); }

fnattrs: fnattrs fnattr { $$ = $1->adopt($2); }
       | { $$ = new ASTNode(cmmParser, CMM_LIST); };

fnattr: "#naked";

block: "{" statements "}" { $$ = $2; D($1, $3); };

statements: statements statement { $$ = $1->adopt($2); }
          | { $$ = new ASTNode(cmmParser, CMM_BLOCK); };

conditional: "if" "(" expr ")" statement "else" statement { $$ = $1->adopt({$3, $5, $7}); D($2, $4, $6); }
           | "if" "(" expr ")" statement { $$ = $1->adopt({$3, $5}); D($2, $4); };

while_loop: "while" expr block { $$ = $1->adopt({$2, $3}); };

for_loop: "for" _decl_or_def ";" _expr ";" _expr block { $$ = $1->adopt({$2, $4, $6, $7}); D($3, $5); };

_expr: expr | { $$ = new ASTNode(cmmParser, CMM_EMPTY); };
_decl_or_def: decl_or_def | { $$ = new ASTNode(cmmParser, CMM_EMPTY); };

expr: expr "&&" expr { $$ = $2->adopt({$1, $3}); }
    | expr "||" expr { $$ = $2->adopt({$1, $3}); }
    | expr "&"  expr %prec BITWISE_AND { $$ = $2->adopt({$1, $3}); }
    | expr "|"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "==" expr { $$ = $2->adopt({$1, $3}); }
    | expr "!=" expr { $$ = $2->adopt({$1, $3}); }
    | expr "<<" expr { $$ = $2->adopt({$1, $3}); }
    | expr ">>" expr { $$ = $2->adopt({$1, $3}); }
    | expr "<"  expr { $$ = $2->adopt({$1, $3}); }
    | expr ">"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "<=" expr { $$ = $2->adopt({$1, $3}); }
    | expr ">=" expr { $$ = $2->adopt({$1, $3}); }
    | expr "+"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "-"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "*"  expr %prec MULT { $$ = $2->adopt({$1, $3}); }
    | expr "/"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "%"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "."  CMMTOK_IDENT %prec "."  { $$ = $2->adopt({$1, $3}); }
    | expr "->" CMMTOK_IDENT %prec "->" { $$ = $2->adopt({$1, $3}); }
    | expr "[" expr "]" %prec "[" { $$ = $2->adopt({$1, $3}); D($4); }
    | function_call %prec CALL
    | "(" expr ")" %prec "(" { $$ = $2; D($1, $3); }
    | "(" type ")" expr %prec CAST { $1->symbol = CMM_CAST; $$ = $1->adopt({$2, $4}); D($3); }
    | "!" expr { $$ = $1->adopt($2); }
    | "~" expr { $$ = $1->adopt($2); }
    | "#" expr { $$ = $1->adopt($2); }
    | "sizeof" "(" type ")" %prec "sizeof" { $$ = $1->adopt($3); D($2, $4); }
    | expr "?" expr ":" expr %prec "?" { $$ = $2->adopt({$1, $3, $5}); D($4); }
    | number
    | "-" number %prec UNARY   { $$ = $2->adopt($1); }
    | "&" expr   %prec ADDROF  { $$ = $1->adopt($2); $$->symbol = CMM_ADDROF; }
    | "*" expr   %prec DEREF   { $$ = $1->adopt($2); }
    | "++" expr  %prec PREFIX  { $$ = $1->adopt($2); }
    | expr "++"  %prec POSTFIX { $$ = $2->adopt($1); $$->symbol = CMM_POSTPLUS; }
    | "--" expr  %prec PREFIX  { $$ = $1->adopt($2); }
    | expr "--"  %prec POSTFIX { $$ = $2->adopt($1); $$->symbol = CMM_POSTMINUS; }
    | ident
    | boolean
    | string
    | CMMTOK_CHAR
    | "null";

string: CMMTOK_STRING;

boolean: "true" | "false";

function_call: ident "(" _exprlist ")" { $$ = $2->adopt({$1, $3}); D($4); };
             | "(" expr ")" "(" _exprlist ")" { $$ = $1->adopt({$2, $5}); D($3, $4, $6); };

exprlist: exprlist "," expr { $$ = $1->adopt($3); D($2); }
        | expr { $$ = (new ASTNode(cmmParser, CMM_LIST))->locate($1)->adopt($1); };

_exprlist: exprlist | { $$ = new ASTNode(cmmParser, CMM_LIST); };

signed_type:   "s8" | "s16" | "s32" | "s64";
unsigned_type: "u8" | "u16" | "u32" | "u64";
int_type: signed_type | unsigned_type;

pointer_type: type "*" { $$ = $2->adopt($1); };

array_type: type "[" expr "]" { $$ = $2->adopt({$1, $3}); D($4); };

fnptr_type: type "(" _typelist ")" "*" { $$ = (new ASTNode(cmmParser, CMM_FNPTR))->locate($1)->adopt({$1, $3}); D($2, $4, $5); };

typelist: typelist "," type { $$ = $1->adopt($3); D($2); }
        | type { $$ = (new ASTNode(cmmParser, CMM_LIST))->locate($1)->adopt($1); };

_typelist: typelist | { $$ = new ASTNode(cmmParser, CMM_LIST); };

struct_type: "struct" CMMTOK_IDENT { $$ = $1->adopt($2); };

type: "bool" | int_type | "void" | pointer_type | array_type | fnptr_type | struct_type;

arg: type ident { $$ = $2->adopt($1); };

arglist: arglist "," arg { $$ = $1->adopt($3); D($2); }
       | arg { $$ = (new ASTNode(cmmParser, CMM_LIST))->locate($1)->adopt($1); };

_arglist: arglist | { $$ = new ASTNode(cmmParser, CMM_LIST); };

number: CMMTOK_NUMBER;
ident:  CMMTOK_IDENT;

%%

#pragma GCC diagnostic pop

const char * Parser::getNameCMM(int symbol) {
    return yytname[YYTRANSLATE(symbol)];
}
