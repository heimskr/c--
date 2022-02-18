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
%token CMMTOK_NEQ "!="
%token CMMTOK_LBRACE "{"
%token CMMTOK_RBRACE "}"
%token CMMTOK_LSQUARE "["
%token CMMTOK_RSQUARE "]"
%token CMMTOK_COMMA ","
%token CMMTOK_QUESTION "?"
%token CMMTOK_COLON ":"
%token CMMTOK_HASH "#"
%token CMMTOK_FN "fn"
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

%token CMM_LIST CMM_ACCESS CMM_BLOCK CMM_CAST CMM_ADDROF

%start start

%left ";"
%right "?" "="
%left "||"
%left "&&"
%left "|"
%left "^"
%left "&"
%left "==" "!="
%left "<" "<=" ">" ">="
%left "<<" ">>"
%left "+" "-"
%left "*" "/"
%left "["
%left "!"
%nonassoc "else"

%%

start: program;

program: program decl_or_def ";" { $$ = $1->adopt($2); D($3); }
       | program function_def { $$ = $1->adopt($2); }
       | program meta { $$ = $1->adopt($2); }
       | { $$ = cmmParser.root; };

meta_start: "#name" | "#author" | "#orcid" | "#version";

meta: meta_start string { $$ = $1->adopt($2); };

statement: block
         | conditional
         | loop
         | decl_or_def
         | expr
         | "return" expr { $$ = $1->adopt($2); };

declaration: ident ":" type { $$ = $2->adopt({$1, $3}); }
definition:  ident ":" type "=" expr { $$ = $2->adopt({$1, $3, $5}); D($4); };
decl_or_def: declaration | definition;

function_def: "fn" ident "(" _arglist ")" ":" type block { $$ = $1->adopt({$2, $7, $4, $8}); D($3, $5, $6); }

block: "{" statements "}" { $$ = $2; D($1, $3); };

statements: statements statement ";" { $$ = $1->adopt($2); D($3); }
          | { $$ = new ASTNode(cmmParser, CMM_BLOCK); };

conditional: "if" expr block "else" block { $$ = $1->adopt({$2, $3, $5}); D($4); }
           | "if" expr block { $$ = $1->adopt({$2, $3}); };

loop: "while" expr block { $$ = $1->adopt({$2, $3}); };

expr: expr "&&" expr { $$ = $2->adopt({$1, $3}); }
    | expr "||" expr { $$ = $2->adopt({$1, $3}); }
    | expr "&"  expr { $$ = $2->adopt({$1, $3}); }
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
    | expr "*"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "/"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "="  expr { $$ = $2->adopt({$1, $3}); }
    // | expr "[" expr "]" { ($$ = $2->adopt({$1, $3}))->symbol = CMM_ACCESS; D($4); }
    | function_call
    | "(" expr ")" { $$ = $2; D($1, $3); }
    | "(" type ")" expr %prec "!" { $1->symbol = CMM_CAST; $$ = $1->adopt({$2, $4}); D($3); }
    | "!" expr { $$ = $1->adopt($2); }
    | number
    | "-" number { $$ = $2->adopt($1); }
    | "&" expr { $$ = $1->adopt($2); $$->symbol = CMM_ADDROF; }
    | "*" expr { $$ = $1->adopt($2); }
    | ident
    | boolean
    | string
    | CMMTOK_CHAR;

string: CMMTOK_STRING;

boolean: "true" | "false";

function_call: ident "(" _exprlist ")" { $$ = $2->adopt({$1, $3}); D($4); };

exprlist: exprlist "," expr { $$ = $1->adopt($3); D($2); }
         | expr { $$ = (new ASTNode(cmmParser, CMM_LIST))->locate($1)->adopt($1); };

_exprlist: exprlist | { $$ = new ASTNode(cmmParser, CMM_LIST); };

signed_type:   "s8" | "s16" | "s32" | "s64";
unsigned_type: "u8" | "u16" | "u32" | "u64";
int_type: signed_type | unsigned_type;

pointer_type: type "*" { $$ = $2->adopt($1); };

type: "bool" | int_type | "void" | pointer_type;

arg: ident ":" type { $$ = $1->adopt($3); D($2); };

arglist: arglist "," arg { $$ = $1->adopt($3); D($2); }
       | arg { $$ = (new ASTNode(cmmParser, CMM_LIST))->locate($1)->adopt($1); };

_arglist: arglist | { $$ = new ASTNode(cmmParser, CMM_LIST); };

number: CMMTOK_NUMBER;
ident:  CMMTOK_IDENT;

%%

#pragma GCC diagnostic pop

const char * Parser::getName(int symbol) {
    return yytname[YYTRANSLATE(symbol)];
}
