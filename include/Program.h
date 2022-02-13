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

	Program(const decltype(globals) &globals_ = {}, const decltype(globalOrder) &global_order = {},
	const decltype(signatures) &signatures_ = {}, const decltype(functions) &functions_ = {}):
		globals(globals_), globalOrder(global_order), signatures(signatures_), functions(functions_) {}
};

Program compileRoot(const ASTNode &);
