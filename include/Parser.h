#pragma once

#include <cstdio>
#include <string>

#include "ASTNode.h"

typedef struct yy_buffer_state * YY_BUFFER_STATE;

class Parser {
	private:
		std::string filename;
		char *buffer = nullptr;
		YY_BUFFER_STATE bufferState = nullptr;

	public:
		enum class Type {Cmm, Wasm};

		ASTNode *root = nullptr;
		int errorCount = 0;
		Type type;

		Parser(Type type_): type(type_) {}
		void open(const std::string &filename);
		void in(const std::string &text);
		void debug(bool flex, bool bison);
		void parse();
		void done();

		const char * getNameCMM(int symbol);
		const char * getNameWASM(int symbol);
		const char * getName(int symbol);
		std::string getBuffer() const;
};

extern Parser cmmParser, wasmParser;
