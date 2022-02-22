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

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
std::string demangle(const char *name) {
	int status = -4;
	std::unique_ptr<char, void(*)(void *)> res(abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free);
	return status == 0? res.get() : name;
}
#else
std::string demangle(const char *name) {
	return name;
}
#endif

int main(int argc, char **argv) {
	if (argc <= 1) {
		std::cerr << "Usage: c-- <input>\n";
		return 1;
	}

	const std::string input = Util::read(argv[1]);

	cmmParser.in(input);
	cmmParser.debug(false, false);
	cmmParser.parse();

	if (cmmParser.errorCount == 0) {
		try {
			Program program = compileRoot(*cmmParser.root);
			program.compile();
			for (const std::string &line: program.lines)
				std::cout << line << '\n';
		} catch (std::exception &err) {
			std::cerr << "\e[38;5;88;1m    ..............\n\e[38;5;196;1m   ::::::::::::::::::\n\e[38;5;202;1m  :::::::::::::::\n\e[38;5;208;1m :::`::::::: :::     :    \e[0;31m"  << demangle(typeid(err).name()) << "\e[0;38;5;208;1m" "\n\e[38;5;142;1m :::: ::::: :::::    :    \e[0m" << err.what() << "\e[0m\e[38;5;142;1m" "\n\e[38;5;40;1m :`   :::::;     :..~~    \e[0m\e[38;5;40;1m" "\n\e[38;5;44;1m :   ::  :::.     :::.\n\e[38;5;39;1m :...`:, :::::...:::\n\e[38;5;27;1m::::::.  :::::::::'      \e[0m\e[38;5;27;1m" "\n\e[38;5;92;1m ::::::::|::::::::  !\n\e[38;5;88;1m :;;;;;;;;;;;;;;;;']}\n\e[38;5;196;1m ;--.--.--.--.--.-\n\e[38;5;202;1m  \\/ \\/ \\/ \\/ \\/ \\/\n\e[38;5;208;1m     :::       ::::\n\e[38;5;142;1m      :::      ::\n\e[38;5;40;1m     :\\:      ::\n\e[38;5;44;1m   /\\::    /\\:::    \n\e[38;5;39;1m ^.:^:.^^^::`::\n\e[38;5;27;1m ::::::::.::::\n\e[38;5;92;1m  .::::::::::\n";
		}
	}

	cmmParser.done();
	success() << "Done.\n";
}
