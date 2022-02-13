#include <fstream>
#include <iostream>
#include <string>

#include "Lexer.h"
#include "Parser.h"

void compile(const ASTNode &);

int main(int argc, char **argv) {
	if (argc <= 1) {
		std::cerr << "Usage: c-- <input>\n";
		return 1;
	}

	std::ifstream file(argv[1]);
	if (!file.is_open())
		throw std::runtime_error("Couldn't open file for reading");
	std::string input;
	file.seekg(0, std::ios::end);
	input.reserve(file.tellg());
	file.seekg(0, std::ios::beg);
	input.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	cmmParser.in(input);
	cmmParser.debug(false, false);
	cmmParser.parse();
	compile(*cmmParser.root);
	cmmParser.done();
}

void compile(const ASTNode &node) {
	node.debug();
}
