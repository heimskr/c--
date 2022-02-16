#include <iostream>

#include "ASTNode.h"
#include "CmmInstructions.h"
#include "Function.h"
#include "Lexer.h"
#include "Scope.h"
#include "Util.h"
#include "WhyInstructions.h"

Function::Function(Program &program_, const ASTNode *source_):
program(program_), source(source_),
selfScope(MultiScope::make(GlobalScope::make(program), FunctionScope::make(*this))) {
	if (source) {
		name = *source->at(0)->lexerInfo;
		for (const ASTNode *child: *source->at(3)) {
			if (child->symbol == CMMTOK_COLON) {
				const std::string &var_name = *child->at(0)->lexerInfo;
				if (variables.count(var_name) != 0)
					throw std::runtime_error("Cannot redefine variable " + var_name + " in function " + name);
				variables.insert({var_name,
					Variable::make(var_name, TypePtr(Type::get(*child->at(1))), this)});
			}
		}
	}
}

std::vector<std::string> Function::stringify() {
	// std::vector<std::string> out {std::to_string(cmm.size()) + ", " + std::to_string(why.size())};
	std::vector<std::string> out;
	for (const auto &instruction: why)
		for (const std::string &line: std::vector<std::string>(*instruction))
			out.push_back(line);
	return out;
}

void Function::compile() {
	if (!source)
		throw std::runtime_error("Can't compile " + name + ": no source node");

	if (source->size() != 4)
		throw std::runtime_error("Expected 4 nodes in " + name + "'s source node, found " +
			std::to_string(source->size()));

	std::cerr << "\n\e[32mCompiling " << name << "\e[39m\n\n";

	returnType = TypePtr(Type::get(*source->at(1)));

	int i = 0;
	for (const ASTNode *child: *source->at(2)) {
		const std::string &argument_name = *child->lexerInfo;
		arguments.push_back(argument_name);
		VariablePtr argument = Variable::make(argument_name, TypePtr(Type::get(*child->front())), this);
		if (i < Why::argumentCount) {
			argument->reg = Why::argumentOffset + i;
		} else
			throw std::runtime_error("Functions with greater than " + std::to_string(Why::argumentCount) + " arguments "
				"are currently unsupported.");
		if (variables.count(argument_name) != 0)
			throw NameConflictError(argument_name);
		variables.emplace(argument_name, argument);
		++i;
	}

	for (const ASTNode *child: *source->at(3)) {
		switch (child->symbol) {
			case CMMTOK_COLON: {
				break;
			}
			default:
				child->debug();
		}
	}
}

VregPtr Function::newVar() {
	return std::make_shared<VirtualRegister>(*this);
}

VregPtr Function::precolored(int reg) {
	auto out = std::make_shared<VirtualRegister>(*this);
	out->reg = reg;
	return out;
}
