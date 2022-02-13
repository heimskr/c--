#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Lexer.h"
#include "Parser.h"
#include "Signature.h"
#include "Type.h"

std::map<std::string, Signature> compileRoot(const ASTNode &);


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
	auto signatures = compileRoot(*cmmParser.root);
	for (const auto &[name, signature]: signatures) {
		std::cerr << "fn " << name << '(';
		bool first = true;
		for (const auto &arg: signature.argumentTypes) {
			if (first)
				first = false;
			else
				std::cerr << ", ";
			std::cerr << std::string(*arg);
		}
		std::cerr << ")" << ": " << std::string(*signature.returnType) << ";\n";
	}
	cmmParser.done();
}

std::map<std::string, Signature> compileRoot(const ASTNode &root) {
	std::map<std::string, Signature> signatures;

	for (const ASTNode *child: root)
		switch (child->symbol) {
			case CMMTOK_FN: {
				const std::string &fn_name = *child->front()->lexerInfo;
				if (signatures.count(fn_name) != 0)
					throw std::runtime_error("Cannot redefine function " + fn_name);
				decltype(Signature::argumentTypes) args;
				for (const ASTNode *arg: *child->at(2))
					args.emplace_back(getType(*arg->front()));
				signatures.try_emplace(fn_name, std::shared_ptr<Type>(getType(*child->at(1))), std::move(args));
				break;
			}
			default: break;
		}

	return signatures;
}
