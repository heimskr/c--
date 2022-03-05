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

%define api.prefix {cpm}

%initial-action {
    cpmParser.root = new ASTNode(cpmParser, CPMTOK_ROOT, {0, 0}, "");
}

%token CPMTOK_ROOT CPMTOK_NUMBER CPMTOK_IDENT CPMTOK_STRING CPMTOK_CHAR
%token CPMTOK_LPAREN "("
%token CPMTOK_RPAREN ")"
%token CPMTOK_PLUS "+"
%token CPMTOK_MINUS "-"
%token CPMTOK_TIMES "*"
%token CPMTOK_DIV "/"
%token CPMTOK_XOR "^"
%token CPMTOK_TRUE "true"
%token CPMTOK_FALSE "false"
%token CPMTOK_IF "if"
%token CPMTOK_ELSE "else"
%token CPMTOK_WHILE "while"
%token CPMTOK_SEMI ";"
%token CPMTOK_DEQ "=="
%token CPMTOK_ASSIGN "="
%token CPMTOK_LAND "&&"
%token CPMTOK_LOR "||"
%token CPMTOK_LXOR "^^"
%token CPMTOK_LNAND "!&&"
%token CPMTOK_LNOR "!||"
%token CPMTOK_LXNOR "!^^"
%token CPMTOK_AND "&"
%token CPMTOK_OR "|"
%token CPMTOK_LTE "<="
%token CPMTOK_GTE ">="
%token CPMTOK_LT "<"
%token CPMTOK_GT ">"
%token CPMTOK_NOT "!"
%token CPMTOK_TILDE "~"
%token CPMTOK_NEQ "!="
%token CPMTOK_LBRACE "{"
%token CPMTOK_RBRACE "}"
%token CPMTOK_LSQUARE "["
%token CPMTOK_RSQUARE "]"
%token CPMTOK_COMMA ","
%token CPMTOK_QUESTION "?"
%token CPMTOK_COLON ":"
%token CPMTOK_HASH "#"
%token CPMTOK_RETURN "return"
%token CPMTOK_PERIOD "."
%token CPMTOK_S8 "s8"
%token CPMTOK_U8 "u8"
%token CPMTOK_S16 "s16"
%token CPMTOK_U16 "u16"
%token CPMTOK_S32 "s32"
%token CPMTOK_U32 "u32"
%token CPMTOK_S64 "s64"
%token CPMTOK_U64 "u64"
%token CPMTOK_BOOL "bool"
%token CPMTOK_VOID "void"
%token CPMTOK_LSHIFT "<<"
%token CPMTOK_RSHIFT ">>"
%token CPMTOK_META_NAME "#name"
%token CPMTOK_META_AUTHOR "#author"
%token CPMTOK_META_VERSION "#version"
%token CPMTOK_META_ORCID "#orcid"
%token CPMTOK_BREAK "break"
%token CPMTOK_CONTINUE "continue"
%token CPMTOK_FOR "for"
%token CPMTOK_MOD "%"
%token CPMTOK_PLUSEQ "+="
%token CPMTOK_MINUSEQ "-="
%token CPMTOK_PLUSPLUS "++"
%token CPMTOK_MINUSMINUS "--"
%token CPMTOK_ASM "asm"
%token CPMTOK_NAKED "#naked"
%token CPMTOK_STRUCT "struct"
%token CPMTOK_ARROW "->"
%token CPMTOK_SIZEOF "sizeof"
%token CPMTOK_NULL "null"
%token CPMTOK_DIVEQ   "/="
%token CPMTOK_TIMESEQ "*="
%token CPMTOK_MODEQ   "%="
%token CPMTOK_SREQ    ">>="
%token CPMTOK_SLEQ    "<<="
%token CPMTOK_ANDEQ   "&="
%token CPMTOK_OREQ    "|="
%token CPMTOK_XOREQ   "^="
%token CPMTOK_CONST "const"
%token CPMTOK_OFFSETOF "offsetof"
%token CPMTOK_SCOPE "::"
%token CPMTOK_STATIC "static"
%token CPMTOK_NEW "new"
%token CPMTOK_DELETE "delete"
%token CPMTOK_CONSTATTR "#const"
%token CPMTOK_OPERATOR "operator"

%token CPM_LIST CPM_ACCESS CPM_BLOCK CPM_CAST CPM_ADDROF CPM_EMPTY CPM_POSTPLUS CPM_POSTMINUS CPM_FNPTR CPM_DECL
%token CPM_INITIALIZER CPM_FNDECL

%start start

%left ";"
%right "?" "=" "+=" "-=" "*=" "/=" "%=" "<<=" ">>=" "&=" "|=" "^="
%left "||"
%left "^^"
%left "&&"
%left "|" "~|"
%left "^" "~^"
%left BITWISE_AND "~&"
%left "==" "!="
%left "<" "<=" ">" ">="
%left "<<" ">>"
%left "+" "-"
%left MULT "/" "%"
%right "!" "~" "#" "sizeof" "offsetof" DEREF ADDROF PREFIX UNARY CAST "new" "delete"
%left "[" POSTFIX CALL "." "->"
%left "::"
%nonassoc "else"

%%

start: program;

program: program decl_or_def ";" { $$ = $1->adopt($2); D($3); }
       | program function_def { $$ = $1->adopt($2); }
       | program function_decl { $$ = $1->adopt($2); }
       | program meta { $$ = $1->adopt($2); }
       | program forward_decl { $$ = $1->adopt($2); }
       | program struct_def { $$ = $1->adopt($2); }
       | { $$ = cpmParser.root; };

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
         | "delete" expr ";" { $$ = $1->adopt($2); D($3); }
         | ";" { $1->symbol = CPM_EMPTY; }
         | inline_asm;

inline_asm: "asm" "(" string ":" _exprlist ":" _exprlist ")" { $$ = $1->adopt({$3, $5, $7}); D($2, $4, $6, $8); }
          | "asm" "(" string "::" _exprlist ")" { $$ = $1->adopt({$3, new ASTNode(cpmParser, CPM_LIST), $5}); D($2, $4, $6); }
          | "asm" "(" string ":" _exprlist ")" { $$ = $1->adopt({$3, $5}); D($2, $4, $6); }
          | "asm" "(" string ")" { $$ = $1->adopt({$3}); D($2, $4); };

declaration: type ident { $$ = (new ASTNode(cpmParser, CPM_DECL, $1->location))->adopt({$1, $2}); }
definition:  type ident "=" expr { $$ = (new ASTNode(cpmParser, CPM_DECL, $1->location))->adopt({$1, $2, $4}); D($3); }
decl_or_def: declaration | definition;

forward_decl: "struct" CPMTOK_IDENT ";" { $$ = $1->adopt($2); D($3); };

struct_def: "struct" CPMTOK_IDENT "{" struct_list "}" ";" { $$ = $1->adopt({$2, $4}); D($3, $5, $6); };

struct_list: struct_list type CPMTOK_IDENT ";" { $$ = $1->adopt($3->adopt($2)); D($4); }
           | struct_list function_decl { $$ = $1->adopt($2); }
           | struct_list "static" function_decl { $$ = $1->adopt($3); $3->attributes.insert("static"); D($2); }
           | struct_list "static" type CPMTOK_IDENT ";" { $$ = $1->adopt($4->adopt($3)); $4->attributes.insert("static"); D($2, $5); }
           | struct_list "static" type CPMTOK_IDENT "=" expr ";" { $$ = $1->adopt($4->adopt({$3, $6})); $4->attributes.insert("static"); D($2, $5, $7); }
           | struct_list "~" ";" { $$ = $1->adopt($2); D($3); }
           | { $$ = new ASTNode(cpmParser, CPM_LIST); };

function_def: type ident "(" _arglist ")" fnattrs block { $$ = $2->adopt({$1, $4, $6, $7}); D($3, $5); }
            | type ident "::" ident "(" _arglist ")" fnattrs block { $$ = $4->adopt({$1, $6, $8, $9, $2}); D($3, $5, $7); }
            | "~" ident fnattrs block { $$ = $1->adopt({new ASTNode(cpmParser, CPMTOK_VOID), new ASTNode(cpmParser, CPM_LIST), $3, $4, $2}); $1->symbol = CPMTOK_IDENT; }
            | "static" type ident "::" ident "(" _arglist ")" fnattrs block { $$ = $5->adopt({$2, $7, $9, $10, $3, $1}); D($4, $6, $8); }
            | type "operator" oper "(" _arglist ")" fnattrs block { $$ = $2->adopt({$1, $3, $5, $7, $8}); D($4, $6); }
            | constructor_def;

oper: "+" | "-" | "*" | "/" | "%" | "&" | "|" | "^" | "!" | "~" | "&&" | "||" | "^^" | "<" | ">" | "<=" | ">=" | "=="
    | "!=" | "=" | "->" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^=" | ">>" | "<<" | ">>=" | "<<=" | "#"
    | "." "++" { $$ = $2; D($1); $2->symbol = CPM_POSTPLUS;  } // postfix ++
    | "." "--" { $$ = $2; D($1); $2->symbol = CPM_POSTMINUS; } // postfix --
    | "++" "." { D($2); } // prefix ++
    | "--" "." { D($2); }
    | "(" ")"  { D($2); }
    | "[" "]"  { D($2); };

constructor_def: "+" ident "(" _arglist ")" fnattrs block { $$ = $1->adopt({$2, $4, $6, $7}); D($3, $5); };

function_decl: type ident "(" _arglist ")" fnattrs ";" { $$ = $2->adopt({$1, $4, $6}); $$->symbol = CPM_FNDECL; D($3, $5, $7); };

fnattrs: fnattrs fnattr { $$ = $1->adopt($2); }
       | { $$ = new ASTNode(cpmParser, CPM_LIST); };

fnattr: "#naked" | "#const";

block: "{" statements "}" { $$ = $2; D($1, $3); };

statements: statements statement { $$ = $1->adopt($2); }
          | { $$ = new ASTNode(cpmParser, CPM_BLOCK); };

conditional: "if" "(" expr ")" statement "else" statement { $$ = $1->adopt({$3, $5, $7}); D($2, $4, $6); }
           | "if" "(" expr ")" statement { $$ = $1->adopt({$3, $5}); D($2, $4); };

while_loop: "while" "(" expr ")" statement { $$ = $1->adopt({$3, $5}); D($2, $4); };

for_loop: "for" "(" _decl_or_def ";" _expr ";" _expr ")" statement { $$ = $1->adopt({$3, $5, $7, $9}); D($2, $4, $6, $8); };

_expr: expr | { $$ = new ASTNode(cpmParser, CPM_EMPTY); };
_decl_or_def: decl_or_def | { $$ = new ASTNode(cpmParser, CPM_EMPTY); };

expr: expr "&&"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "||"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "&"   expr %prec BITWISE_AND { $$ = $2->adopt({$1, $3}); }
    | expr "|"   expr { $$ = $2->adopt({$1, $3}); }
    | expr "^"   expr { $$ = $2->adopt({$1, $3}); }
    | expr "^^"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "=="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "!="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "<<"  expr { $$ = $2->adopt({$1, $3}); }
    | expr ">>"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "<"   expr { $$ = $2->adopt({$1, $3}); }
    | expr ">"   expr { $$ = $2->adopt({$1, $3}); }
    | expr "<="  expr { $$ = $2->adopt({$1, $3}); }
    | expr ">="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "+"   expr { $$ = $2->adopt({$1, $3}); }
    | expr "-"   expr { $$ = $2->adopt({$1, $3}); }
    | expr "*"   expr %prec MULT { $$ = $2->adopt({$1, $3}); }
    | expr "/"   expr { $$ = $2->adopt({$1, $3}); }
    | expr "="   expr { $$ = $2->adopt({$1, $3}); }
    | expr "%"   expr { $$ = $2->adopt({$1, $3}); }
    | expr "+="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "-="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "*="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "/="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "%="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "<<=" expr { $$ = $2->adopt({$1, $3}); }
    | expr ">>=" expr { $$ = $2->adopt({$1, $3}); }
    | expr "&="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "|="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "^="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "."   CPMTOK_IDENT %prec "."  { $$ = $2->adopt({$1, $3}); }
    | expr "->"  CPMTOK_IDENT %prec "->" { $$ = $2->adopt({$1, $3}); }
    | expr "[" expr "]" %prec "[" { $$ = $2->adopt({$1, $3}); D($4); }
    | function_call %prec CALL
    | "(" expr ")" %prec "(" { $$ = $2; D($1, $3); }
    | "(" type ")" expr %prec CAST { $1->symbol = CPM_CAST; $$ = $1->adopt({$2, $4}); D($3); }
    | "!" expr { $$ = $1->adopt($2); }
    | "~" expr { $$ = $1->adopt($2); }
    | "#" expr { $$ = $1->adopt($2); }
    | "sizeof" "(" type ")" %prec "sizeof" { $$ = $1->adopt($3); D($2, $4); }
    | "sizeof" "(" "%" CPMTOK_IDENT "," CPMTOK_IDENT ")" %prec "sizeof" { $$ = $1->adopt({$4, $6}); D($2, $3, $5, $7); }
    | "offsetof" "(" "%" CPMTOK_IDENT "," CPMTOK_IDENT ")" %prec "offsetof" { $$ = $1->adopt({$4, $6}); D($2, $3, $5, $7); }
    | expr "?" expr ":" expr %prec "?" { $$ = $2->adopt({$1, $3, $5}); D($4); }
    | number
    | "-" number %prec UNARY   { $$ = $2->adopt($1); }
    | "&" expr   %prec ADDROF  { $$ = $1->adopt($2); $$->symbol = CPM_ADDROF; }
    | "*" expr   %prec DEREF   { $$ = $1->adopt($2); }
    | "++" expr  %prec PREFIX  { $$ = $1->adopt($2); }
    | expr "++"  %prec POSTFIX { $$ = $2->adopt($1); $$->symbol = CPM_POSTPLUS; }
    | "--" expr  %prec PREFIX  { $$ = $1->adopt($2); }
    | expr "--"  %prec POSTFIX { $$ = $2->adopt($1); $$->symbol = CPM_POSTMINUS; }
    | CPMTOK_IDENT
    | boolean
    | string
    | CPMTOK_CHAR
    | "[" _exprlist "]" { $$ = $2; $$->symbol = CPM_INITIALIZER; D($1, $3); }
    | "%" "[" _exprlist "]" { $$ = $3; $$->symbol = CPM_INITIALIZER; $$->attributes.insert("constructor"); D($1, $2, $4); }
    | "new" new_type "(" _exprlist ")" %prec "new" { $$ = $1->adopt({$2, $4}); D($3, $5); }
    | struct_type "::" CPMTOK_IDENT { $$ = $2->adopt({$1, $3}); }
    | "null";

string: CPMTOK_STRING;

boolean: "true" | "false";

function_call: ident "(" _exprlist ")" { $$ = $2->adopt({$1, $3}); D($4); };
             | "(" expr ")" "(" _exprlist ")" { $$ = $1->adopt({$2, $5}); D($3, $4, $6); }
             | expr "::" ident "(" _exprlist ")" %prec "::" { $$ = $4->adopt({$3, $5, $1}); D($2, $6); }
             | struct_type "::" ident "(" _exprlist ")" %prec "::" { $$ = $4->adopt({$3, $5, $1}); D($2, $6); }
             | constructor_call;

constructor_call: struct_type "(" _exprlist ")" { $$ = $2->adopt({$1, $3}); D($4); };

exprlist: exprlist "," expr { $$ = $1->adopt($3); D($2); }
        | expr { $$ = (new ASTNode(cpmParser, CPM_LIST))->locate($1)->adopt($1); };

_exprlist: exprlist | { $$ = new ASTNode(cpmParser, CPM_LIST); };

signed_type:   "s8" | "s16" | "s32" | "s64";
unsigned_type: "u8" | "u16" | "u32" | "u64";
int_type: signed_type | unsigned_type;

pointer_type: type "*" { $$ = $2->adopt($1); };

array_type: type "[" expr "]" { $$ = $2->adopt({$1, $3}); D($4); };

fnptr_type: type "(" _typelist ")" "*" { $$ = (new ASTNode(cpmParser, CPM_FNPTR))->locate($1)->adopt({$1, $3}); D($2, $4, $5); };

typelist: typelist "," type { $$ = $1->adopt($3); D($2); }
        | type { $$ = (new ASTNode(cpmParser, CPM_LIST))->locate($1)->adopt($1); };

_typelist: typelist | { $$ = new ASTNode(cpmParser, CPM_LIST); };

struct_type: "%" CPMTOK_IDENT { $$ = $1->adopt($2); };

new_type: "bool" | int_type | new_type "*" { $$ = $2->adopt($1); } | struct_type | new_type "const" { $$ = $2->adopt($1); };

type: "bool" | int_type | "void" | pointer_type | array_type | fnptr_type | struct_type | type "const" { $$ = $2->adopt($1); };

arg: type ident { $$ = $2->adopt($1); };

arglist: arglist "," arg { $$ = $1->adopt($3); D($2); }
       | arg { $$ = (new ASTNode(cpmParser, CPM_LIST))->locate($1)->adopt($1); };

_arglist: arglist | { $$ = new ASTNode(cpmParser, CPM_LIST); };

number: CPMTOK_NUMBER;
ident:  CPMTOK_IDENT;

%%

#pragma GCC diagnostic pop

const char * Parser::getNameCPM(int symbol) {
    return yytname[YYTRANSLATE(symbol)];
}
