#pragma once

#include <unordered_map>
#include <vector>

#include <stdio.h>

#include "ASTNode.h"

#define CMMSTYPE_IS_DECLARED
typedef ASTNode * CMMSTYPE;

#define WASMSTYPE_IS_DECLARED
typedef ASTNode * WASMSTYPE;

#ifndef NO_YYPARSE
#include "bison.h"
#include "wasmbison.h"
#endif

#ifdef __APPLE__
typedef size_t yysize;
#else
typedef int yysize;
#endif

extern FILE *cmmin, *wasmin;
extern char *cmmtext, *wasmtext;
extern yysize cmmleng, wasmleng;
extern int cmm_flex_debug, wasm_flex_debug;
extern int cmmdebug, wasmdebug;

class Parser;

class Lexer {
	private:
		Parser *parser;
		yysize *leng;
		ASTNode **lval;

	public:
		ASTLocation location = {0, 1};
		std::string line;
		yysize lastYylength = 0;
		std::unordered_map<int, std::string> lines;
		bool failed = false;
		std::vector<std::pair<std::string, ASTLocation>> errors;

		Lexer(Parser &, yysize &, ASTNode *&);
		const std::string * filename(int fileno);
		void advance(const char *);
		void newline();
		void badchar(unsigned char);
		int token(const char *, int symbol);
};

extern Lexer cmmLexer, wasmLexer;

int cmmlex();
int cmmlex_destroy();
int cmmparse();
void cmmerror(const std::string &);
void cmmerror(const std::string &, const ASTLocation &);

int wasmlex();
int wasmlex_destroy();
int wasmparse();
void wasmerror(const char *);
void wasmerror(const std::string &);
void wasmerror(const std::string &, const ASTLocation &);
