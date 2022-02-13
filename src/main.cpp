#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

	for (const auto &iter: program.globalOrder) {
		std::cerr << "\e[1m" << iter->first << "\e[22;2m:\e[22;36m " << std::string(*iter->second.type) << "\e[39m";
		if (iter->second.value)
			std::cerr << " \e[2m=\e[22m ...";
		std::cerr << "\e[2m;\e[22m\n";
	}

	for (const auto &[name, signature]: program.signatures) {
		std::cerr << "\e[91mfn\e[39;1m " << name << "\e[22;2m(\e[22m";
		bool first = true;
		for (const auto &arg: signature.argumentTypes) {
			if (first)
				first = false;
			else
				std::cerr << "\e[2m,\e[22m ";
			std::cerr << "\e[36m" << std::string(*arg) << "\e[39m";
		}
		std::cerr << "\e[2m):\e[22;36m " << std::string(*signature.returnType) << "\e[39;2m;\e[22m\n";
	}

	cmmParser.done();
}
