#include <iostream>

#include "ASTNode.h"
#include "Errors.h"
#include "Expr.h"
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

	for (const ASTNode *child: *source->at(3))
		compile(*child);

	auto fp = precolored(Why::framePointerOffset), rt = precolored(Why::returnAddressOffset);
	auto sp = precolored(Why::stackPointerOffset);
	addFront<MoveInstruction>(sp, fp);
	// TODO: push all used temporaries here
	addFront<StackPushInstruction>(fp);
	addFront<StackPushInstruction>(rt);
	add<Label>("." + name + "$e");
	// TODO: pop all used temporaries here
	add<StackPopInstruction>(fp);
	add<StackPopInstruction>(rt);
	add<JumpRegisterInstruction>(rt, false);
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

void Function::compile(const ASTNode &node) {
	switch (node.symbol) {
		case CMMTOK_COLON: {
			const std::string &var_name = *node.front()->lexerInfo;
			if (selfScope->lookup(var_name))
				throw NameConflictError(var_name, node.front()->location);
			VariablePtr variable = Variable::make(var_name, TypePtr(Type::get(*node.at(1))), this);
			variables.emplace(var_name, variable);
			addToStack(variable);
			if (node.size() == 3)
				ExprPtr(Expr::get(*node.at(2), this))->compile(variable, *this, selfScope);
			break;
		}
		case CMMTOK_RETURN:
			ExprPtr(Expr::get(*node.front(), this))->compile(precolored(Why::returnValueOffset), *this,
				selfScope);
			add<JumpInstruction>("." + name + "$e");
			break;
		case CMMTOK_LPAREN:
			ExprPtr(Expr::get(node, this))->compile(nullptr, *this, selfScope);
			break;
		case CMMTOK_WHILE: {
			ExprPtr condition = ExprPtr(Expr::get(*node.front(), this));
			const std::string label = "." + name + "$" + std::to_string(nextBlock++);
			const std::string start = label + "s", end = label + "e";
			add<Label>(start);
			auto m0 = mx(0);
			condition->compile(m0, *this, selfScope);
			add<LogicalNotInstruction>(m0);
			add<JumpConditionalInstruction>(end, m0);
			compile(*node.at(1));
			add<JumpInstruction>(start);
			add<Label>(end);
			break;
		}
		case CMM_BLOCK:
			for (const ASTNode *child: node)
				compile(*child);
			break;
		case CMMTOK_IF: {
			addComment("<if>");
			node.debug();
			ExprPtr condition = ExprPtr(Expr::get(*node.front(), this));
			const std::string else_label = "." + name + "$" + std::to_string(nextBlock++) + "e";
			const std::string end_label = else_label + "nd";
			auto m0 = mx(0);
			condition->compile(m0, *this, selfScope);
			add<LogicalNotInstruction>(m0);
			add<JumpConditionalInstruction>(else_label, m0);
			compile(*node.at(1));
			add<JumpInstruction>(end_label);
			add<Label>(else_label);
			compile(*node.at(2));
			add<Label>(end_label);
			addComment("</if>");
			break;
		}
		default:
			node.debug();
			break;
	}
}

void Function::addComment(const std::string &comment) {
	add<Comment>(comment);
}

VregPtr Function::mx(int n) {
	return precolored(Why::assemblerOffset + n);
}
