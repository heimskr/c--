#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Errors.h"
#include "Expr.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
#include "Type.h"
#include "Util.h"

// #define CATCH_COMPILE

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
#ifdef CATCH_COMPILE
		bool should_try = true;
#else
		bool should_try = false;
#endif
		if (2 < argc && strcmp(argv[2], "-d") == 0)
			should_try = true;
		if (should_try) {
			try {
				Program program = compileRoot(*cmmParser.root, argv[1]);
				program.compile();
				for (const std::string &line: program.lines)
					std::cout << line << '\n';
				success() << "Done.\n";
			} catch (std::exception &err) {
				std::cerr << "\e[38;5;88;1m    ..............\n\e[38;5;196;1m   ::::::::::::::::::\n\e[38;5;202;1m  :::::::::::::::\n\e[38;5;208;1m :::`::::::: :::     :    \e[0;31m" << demangle(typeid(err).name()) << "\e[0;38;5;208;1m\n\e[38;5;142;1m :::: ::::: :::::    :    \e[0m" << err.what() << "\e[0m\e[38;5;142;1m\n\e[38;5;40;1m :`   :::::;     :..~~    \e[0m";
				if (auto *located = dynamic_cast<GenericError *>(&err))
					if (located->location)
						std::cerr << "Location: " << located->location;
				std::cerr << "\e[38;5;40;1m\n\e[38;5;44;1m :   ::  :::.     :::.\n\e[38;5;39;1m :...`:, :::::...:::\n\e[38;5;27;1m::::::.  :::::::::'      \e[0m\e[38;5;27;1m\n\e[38;5;92;1m ::::::::|::::::::  !\n\e[38;5;88;1m :;;;;;;;;;;;;;;;;']}\n\e[38;5;196;1m ;--.--.--.--.--.-\n\e[38;5;202;1m  \\/ \\/ \\/ \\/ \\/ \\/\n\e[38;5;208;1m     :::       ::::\n\e[38;5;142;1m      :::      ::\n\e[38;5;40;1m     :\\:      ::\n\e[38;5;44;1m   /\\::    /\\:::    \n\e[38;5;39;1m ^.:^:.^^^::`::\n\e[38;5;27;1m ::::::::.::::\n\e[38;5;92;1m  .::::::::::\n";
			}
		} else {
			Program program = compileRoot(*cmmParser.root, argv[1]);
			program.compile();
			for (const std::string &line: program.lines)
				std::cout << line << '\n';
			success() << "Done.\n";
		}
	}

	cmmParser.done();
}
