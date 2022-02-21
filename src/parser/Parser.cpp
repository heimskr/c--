#include <cstring>
#include <fstream>
#include <sstream>

#include "Parser.h"
#include "Lexer.h"
#include "StringSet.h"

int cmmwrap() { return 1; }

extern YY_BUFFER_STATE cmm_scan_buffer(char *, size_t);
extern void cmm_delete_buffer(YY_BUFFER_STATE);

void Parser::open(const std::string &filename_) {
	errorCount = 0;
	filename = filename_;
	cmmin = fopen(filename.c_str(), "r");
}

void Parser::in(const std::string &text) {
	errorCount = 0;
	buffer = new char[text.size() + 2];
	std::strncpy(buffer, text.c_str(), text.size() + 1);
	buffer[text.size() + 1] = '\0'; // Input to flex needs two null terminators.
	bufferState = cmm_scan_buffer(buffer, text.size() + 2);
}

void Parser::debug(bool flex, bool bison) {
	cmm_flex_debug = flex;
	cmmdebug = bison;
}

void Parser::parse() {
	cmmparse();
}

void Parser::done() {
	cmmlex_destroy();

	if (root) {
		delete root;
		root = nullptr;
	}

	if (buffer) {
		delete buffer;
		buffer = nullptr;
	}
}

const char * Parser::getName(int symbol) {
	switch (type) {
		case Type::Cmm:  return getNameCMM(symbol);
		case Type::Wasm: return getNameWASM(symbol);
		default: throw std::runtime_error("Invalid parser type: " + std::to_string(int(type)));
	}
}

std::string Parser::getBuffer() const {
	return buffer? buffer : "";
}

Parser cmmParser(Parser::Type::Cmm), wasmParser(Parser::Type::Wasm);
