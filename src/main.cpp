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

Program compileRoot(const ASTNode &);

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
	Program program = compileRoot(*cmmParser.root);
	for (const auto &iter: program.globalOrder) {
		std::cerr << iter->first << ": " << std::string(*iter->second.type);
		if (iter->second.value)
			std::cerr << " = ...";
		std::cerr << ";\n";
	}
	for (const auto &[name, signature]: program.signatures) {
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

Program compileRoot(const ASTNode &root) {
	std::map<std::string, Global> globals;
	std::vector<std::map<std::string, Global>::iterator> global_order;
	std::map<std::string, Signature> signatures;

	for (const ASTNode *child: root)
		switch (child->symbol) {
			case CMMTOK_FN: {
				const std::string &name = *child->front()->lexerInfo;
				if (signatures.count(name) != 0)
					throw std::runtime_error("Cannot redefine function " + name);
				decltype(Signature::argumentTypes) args;
				for (const ASTNode *arg: *child->at(2))
					args.emplace_back(getType(*arg->front()));
				signatures.try_emplace(name, std::shared_ptr<Type>(getType(*child->at(1))), std::move(args));
				break;
			}
			case CMMTOK_COLON: { // Global variable
				const std::string &name = *child->front()->lexerInfo;
				if (globals.count(name) != 0)
					throw std::runtime_error("Cannot redefine global " + name);
				if (child->size() <= 2)
					global_order.push_back(globals.try_emplace(name, name,
						std::shared_ptr<Type>(getType(*child->at(1))), nullptr).first);
				else
					global_order.push_back(globals.try_emplace(name, name,
						std::shared_ptr<Type>(getType(*child->at(1))), child->at(2)).first);
				break;
			}
			default:
				throw std::runtime_error("Unexpected token under root: " +
					std::string(cmmParser.getName(child->symbol)));
		}

	return {globals, global_order, signatures};
}
