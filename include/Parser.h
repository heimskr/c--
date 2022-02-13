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
		ASTNode *root = nullptr;
		int errorCount = 0;

		Parser() = default;
		void open(const std::string &filename);
		void in(const std::string &text);
		void debug(bool flex, bool bison);
		void parse();
		void done();

		const char * getName(int symbol);
		std::string getBuffer() const;
};

extern Parser cmmParser;
