#pragma once

#include <map>
#include <string>
#include <vector>

#include "Function.h"
#include "Global.h"
#include "Signature.h"

class ASTNode;

struct Program {
	std::map<std::string, Global> globals;
	std::vector<std::map<std::string, Global>::iterator> globalOrder;
	std::map<std::string, Signature> signatures;
	std::map<std::string, Function> functions;
	std::vector<std::string> lines;
	Function init {*this, nullptr};

	Program() = default;

	Program(decltype(globals) &&globals_, decltype(globalOrder) &&global_order, decltype(signatures) &&signatures_,
	decltype(functions) &&functions_):
		globals(std::move(globals_)), globalOrder(std::move(global_order)), signatures(std::move(signatures_)),
		functions(std::move(functions_)) {}

	void compile();
};

Program compileRoot(const ASTNode &);
