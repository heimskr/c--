#pragma once

#include <cstdio>
#include <string>

#include "ASTNode.h"

using YY_BUFFER_STATE = struct yy_buffer_state *;

class Parser {
	private:
		std::string filename;
		char *buffer = nullptr;
		YY_BUFFER_STATE bufferState = nullptr;

	public:
		enum class Type {Cpm, Wasm};

		ASTNode *root = nullptr;
		int errorCount = 0;
		Type type;

		explicit Parser(Type type_): type(type_) {}
		void open(const std::string &filename);
		void in(const std::string &text);
		void debug(bool flex, bool bison);
		void parse();
		void done();

		const char * getNameCPM(int symbol);
		const char * getNameWASM(int symbol);
		const char * getName(int symbol);
		[[nodiscard]] std::string getBuffer() const;
};

extern Parser cpmParser, wasmParser;
