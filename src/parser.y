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

%token CMMTOK_ROOT CMMTOK_NUMBER CMMTOK_IDENT
%token CMMTOK_LPAREN "("
%token CMMTOK_RPAREN ")"
%token CMMTOK_PLUS "+"
%token CMMTOK_MINUS "-"
%token CMMTOK_TIMES "*"
%token CMMTOK_DIV "/"
%token CMMTOK_POW "^"
%token CMMTOK_TRUE "true"
%token CMMTOK_FALSE "false"
%token CMMTOK_IF "if"
%token CMMTOK_THEN "then"
%token CMMTOK_ELSE "else"
%token CMMTOK_WHILE "while"
%token CMMTOK_DO "do"
%token CMMTOK_SEMI ";"
%token CMMTOK_DEQ "=="
%token CMMTOK_ASSIGN "="
%token CMMTOK_SKIP "skip"
%token CMMTOK_LAND "&&"
%token CMMTOK_LOR "||"
%token CMMTOK_AND "&"
%token CMMTOK_OR "|"
%token CMMTOK_LTE "<="
%token CMMTOK_GTE ">="
%token CMMTOK_LT "<"
%token CMMTOK_GT ">"
%token CMMTOK_NOT "¬"
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
%token CMMTOK_PUSH "push"
%token CMMTOK_PERIOD "."
%token CMMTOK_POP "pop"
%token CMMTOK_INSERT "insert"
%token CMMTOK_ERASE "erase"

%token CMM_LIST CMM_ACCESS

%start start

%left ";"
%right "?"
%left "∨"
%left "∧"
%left "<" "<=" ">" ">=" "=" "!="
%left "+" "-"
%left "*" "/"
%left "^"
%left "["
%left "¬" "#" UNARY

%%

start: program;

program: superstatement { $$ = cmmParser.root->adopt($1); };

superstatement: superstatement ";" statement { $$ = $2->adopt({$1, $3}); }
              | statement;

method: "push" | "pop" | "insert" | "erase";

statement: "{" superstatement "}" { $$ = $2; D($1, $3); }
         | "{" "}" { $$ = $1; D($2); }
         | assignment
         | conditional
         | loop
         | "fn" ident "(" _identlist ")" "{" superstatement "}" { $$ = $1->adopt({$2, $7, $4}); D($3, $5, $6, $8); }
         | "return" expr { $$ = $1->adopt($2); }
         | expr "." method "(" _exprlist ")" { $$ = $3->adopt({$1, $5}); D($2, $4, $6); }
         | function_call
         | "skip";

assignment: expr ":=" expr { $$ = $2->adopt({$1, $3}); };

conditional: "if" expr "then" statement "else" statement { $$ = $1->adopt({$2, $4, $6}); D($3, $5); };

loop: "while" expr "do" statement { $$ = $1->adopt({$2, $4}); D($3); };

expr: expr "?" expr ":" expr %prec "?" { $$ = $2->adopt({$1, $3, $5}); D($4); }
    | expr "∨"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "∧"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "="  expr { $$ = $2->adopt({$1, $3}); }
    | expr "!=" expr { $$ = $2->adopt({$1, $3}); }
    | expr "<"  expr { $$ = $2->adopt({$1, $3}); }
    | expr ">"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "<=" expr { $$ = $2->adopt({$1, $3}); }
    | expr ">=" expr { $$ = $2->adopt({$1, $3}); }
    | expr "+"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "-"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "*"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "/"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "^"  expr { $$ = $2->adopt({$1, $3}); }
    | expr "[" expr "]" { ($$ = $2->adopt({$1, $3}))->symbol = CMM_ACCESS; D($4); }
    | function_call
    | "(" expr ")" { $$ = $2; D($1, $3); }
    | "¬" expr { $$ = $1->adopt($2); }
    | "#" expr { $$ = $1->adopt($2); }
    | number
    | "-" number %prec UNARY { $$ = $2->adopt($1); }
    | ident
    | "true";
    | "false"
    | array_literal;

function_call: ident "(" _exprlist ")" { $$ = $2->adopt({$1, $3}); D($4); };

array_literal: "[" _exprlist "]" { $$ = $1->adopt($2); D($3); };

exprlist: exprlist "," expr { $$ = $1->adopt($3); D($2); }
         | expr { $$ = (new ASTNode(cmmParser, CMM_LIST))->locate($1)->adopt($1); };

_exprlist: exprlist | { $$ = new ASTNode(cmmParser, CMM_LIST); };

identlist: identlist "," ident { $$ = $1->adopt($3); D($2); }
         | ident { $$ = (new ASTNode(cmmParser, CMM_LIST))->locate($1)->adopt($1); };

_identlist: identlist | { $$ = new ASTNode(cmmParser, CMM_LIST); };

number: CMMTOK_NUMBER;
ident:  CMMTOK_IDENT | method;

%%

#pragma GCC diagnostic pop

const char * Parser::getName(int symbol) {
    return yytname[YYTRANSLATE(symbol)];
}
