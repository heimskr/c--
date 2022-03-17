#pragma once

#include <unordered_map>
#include <vector>

#include <stdio.h>

#include "ASTNode.h"

#define CPMSTYPE_IS_DECLARED
typedef ASTNode * CPMSTYPE;

#define WASMSTYPE_IS_DECLARED
typedef ASTNode * WASMSTYPE;

#ifndef NO_YYPARSE
#include "bison.h"
#include "wasmbison.h"
#endif

#ifdef __APPLE__
using yysize = size_t;
#else
using yysize = int;
#endif

extern FILE *cpmin, *wasmin;
extern char *cpmtext, *wasmtext;
extern yysize cpmleng, wasmleng;
extern int cpm_flex_debug, wasm_flex_debug;

class Parser;

class Lexer {
	private:
		Parser *parser;
		yysize *leng;
		ASTNode **lval;

	public:
		ASTLocation location {0, 1};
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

extern Lexer cpmLexer, wasmLexer;

int cpmlex();
int cpmlex_destroy();
void cpmerror(const std::string &);
void cpmerror(const std::string &, const ASTLocation &);

int wasmlex();
int wasmlex_destroy();
void wasmerror(const char *);
void wasmerror(const std::string &);
void wasmerror(const std::string &, const ASTLocation &);
