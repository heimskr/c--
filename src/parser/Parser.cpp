#include <cstring>
#include <fstream>
#include <sstream>

#include "Parser.h"
#include "Lexer.h"
#include "StringSet.h"

int cpmwrap() { return 1; }
int wasmwrap() { return 1; }

extern YY_BUFFER_STATE cpm_scan_buffer(char *, size_t);
extern YY_BUFFER_STATE wasm_scan_buffer(char *, size_t);
extern void cpm_delete_buffer(YY_BUFFER_STATE);
extern void wasm_delete_buffer(YY_BUFFER_STATE);

void Parser::open(const std::string &filename_) {
	errorCount = 0;
	filename = filename_;
	if (type == Type::Cpm)
		cpmin = fopen(filename.c_str(), "r");
	else
		wasmin = fopen(filename.c_str(), "r");
}

void Parser::in(const std::string &text) {
	errorCount = 0;
	buffer = new char[text.size() + 2];
	std::strncpy(buffer, text.c_str(), text.size() + 1);
	buffer[text.size() + 1] = '\0'; // Input to flex needs two null terminators.
	if (type == Type::Cpm)
		bufferState = cpm_scan_buffer(buffer, text.size() + 2);
	else
		bufferState = wasm_scan_buffer(buffer, text.size() + 2);
}

void Parser::debug(bool flex, bool bison) {
	if (type == Type::Cpm) {
		cpm_flex_debug = flex;
		cpmdebug = bison;
	} else {
		wasm_flex_debug = flex;
		wasmdebug = bison;
	}
}

void Parser::parse() {
	if (type == Type::Cpm)
		cpmparse();
	else
		wasmparse();
}

void Parser::done() {
	if (type == Type::Cpm)
		cpmlex_destroy();
	else
		wasmlex_destroy();

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
		case Type::Cpm:  return getNameCPM(symbol);
		case Type::Wasm: return getNameWASM(symbol);
		default: throw std::runtime_error("Invalid parser type: " + std::to_string(int(type)));
	}
}

std::string Parser::getBuffer() const {
	return buffer? buffer : "";
}

Parser cpmParser(Parser::Type::Cpm), wasmParser(Parser::Type::Wasm);
