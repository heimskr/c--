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
		returnType = TypePtr(Type::get(*source->at(1)));
	} else
		returnType = VoidType::make();
	scopes.emplace(source, selfScope);
}

std::vector<std::string> Function::stringify() {
	std::vector<std::string> out;
	for (const auto &instruction: why)
		for (const std::string &line: std::vector<std::string>(*instruction))
			out.push_back(line);
	return out;
}

void Function::compile() {
	if (isBuiltin())
		return;

	if (!source)
		throw std::runtime_error("Can't compile " + name + ": no source node");

	if (source->size() != 4)
		throw std::runtime_error("Expected 4 nodes in " + name + "'s source node, found " +
			std::to_string(source->size()));

	std::cerr << "\n\e[32mCompiling " << name << "\e[39m\n\n";

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
		if (selfScope->lookup(argument_name))
			throw NameConflictError(argument_name);
		variables.emplace(argument_name, argument);
		++i;
	}

	for (const ASTNode *child: *source->at(3)) {
		switch (child->symbol) {
			case CMMTOK_COLON: {
				const std::string &var_name = *child->front()->lexerInfo;
				if (selfScope->lookup(var_name))
					throw NameConflictError(var_name, child->front()->location);
				VariablePtr variable = Variable::make(var_name, TypePtr(Type::get(*child->at(1))), this);
				variables.emplace(var_name, variable);
				addToStack(variable);
				if (child->size() == 3)
					ExprPtr(Expr::get(*child->at(2), this))->compile(variable, *this, selfScope);
				break;
			}
			case CMMTOK_RETURN: {
				VregPtr return_value = precolored(Why::returnValueOffset);
				ExprPtr(Expr::get(*child->front(), this))->compile(return_value, *this, selfScope);
				break;
			}
			case CMMTOK_LPAREN:
				ExprPtr(Expr::get(*child, this))->compile(precolored(Why::assemblerOffset), *this, selfScope);
				break;
			default:
				child->debug();
				break;
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

void Function::addToStack(VariablePtr variable) {
	if (stackOffsets.count(variable) != 0)
		throw std::runtime_error("Variable already on the stack in function " + name + ": " + variable->name);
	stackOffsets.emplace(variable, stackUsage);
	stackUsage += variable->type->getSize();
}
