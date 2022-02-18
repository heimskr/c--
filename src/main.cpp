#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Expr.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
#include "Type.h"
#include "Util.h"

int main(int argc, char **argv) {
	if (argc <= 1) {
		std::cerr << "Usage: c-- <input>\n";
		return 1;
	}

	std::string input = Util::read(argv[1]);

	cmmParser.in(input);
	cmmParser.debug(false, false);
	cmmParser.parse();

	Program program = compileRoot(*cmmParser.root);
	program.compile();

	for (const std::string &line: program.lines)
		std::cout << line << '\n';

	cmmParser.done();
}
