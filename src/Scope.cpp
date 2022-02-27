#include "Function.h"
#include "Program.h"
#include "Scope.h"

VariablePtr EmptyScope::lookup(const std::string &) const {
	return nullptr;
}

bool EmptyScope::doesConflict(const std::string &) const {
	return false;
}

bool EmptyScope::insert(VariablePtr) {
	return false;
}

VariablePtr BasicScope::lookup(const std::string &name) const {
	if (variables.count(name) == 0)
		return nullptr;
	return variables.at(name);
}

bool BasicScope::doesConflict(const std::string &name) const {
	return variables.count(name) != 0;
}

bool BasicScope::insert(VariablePtr variable) {
	if (doesConflict(variable->name))
		return false;
	variables.emplace(variable->name, variable);
	return true;
}

FunctionScope::FunctionScope(Function &function_, std::shared_ptr<GlobalScope> parent_):
	Scope(&function_.program), function(function_), parent(parent_) {}

VariablePtr FunctionScope::lookup(const std::string &name) const {
	if (function.variables.count(name) == 0)
		return parent->lookup(name);
	return function.variables.at(name);
}

FunctionPtr FunctionScope::lookupFunction(const std::string &name) const {
	return parent->lookupFunction(name);
}

TypePtr FunctionScope::lookupType(const std::string &name) const {
	return parent->lookupType(name);
}

bool FunctionScope::doesConflict(const std::string &name) const {
	// It's fine for local variables to shadow global variables.
	return function.variables.count(name) != 0;
}

bool FunctionScope::insert(VariablePtr variable) {
	if (doesConflict(variable->name))
		return false;
	function.variables.emplace(variable->name, variable);
	return true;
}

VariablePtr GlobalScope::lookup(const std::string &name) const {
	if (program.globals.count(name) == 0)
		return nullptr;
	return program.globals.at(name);
}

FunctionPtr GlobalScope::lookupFunction(const std::string &name) const {
	if (program.functions.count(name) == 0) {
		if (program.functionDeclarations.count(name) == 0)
			return nullptr;
		return program.functionDeclarations.at(name);
	}
	return program.functions.at(name);
}

TypePtr GlobalScope::lookupType(const std::string &name) const {
	if (program.structs.count(name) != 0)
		return program.structs.at(name);
	return nullptr;
}

bool GlobalScope::doesConflict(const std::string &name) const {
	return program.globals.count(name) != 0;
}

bool GlobalScope::insert(VariablePtr variable) {
	if (!variable->is<Global>())
		throw std::invalid_argument("Can't insert a non-global into a GlobalScope");

	if (doesConflict(variable->name))
		return false;

	program.globals.emplace(variable->name, std::dynamic_pointer_cast<Global>(variable));
	return true;
}

VariablePtr MultiScope::lookup(const std::string &name) const {
	for (const auto &scope: scopes)
		if (auto var = scope->lookup(name))
			return var;
	return nullptr;
}

FunctionPtr MultiScope::lookupFunction(const std::string &name) const {
	for (const auto &scope: scopes)
		if (auto function = scope->lookupFunction(name))
			return function;
	return nullptr;
}

bool MultiScope::doesConflict(const std::string &name) const {
	for (const auto &scope: scopes)
		if (scope->doesConflict(name))
			return true;
	return false;
}

bool MultiScope::insert(VariablePtr variable) {
	if (scopes.empty())
		throw std::runtime_error("Can't insert variable into an empty MultiScope");

	if (doesConflict(variable->name))
		return false;

	scopes.front()->insert(variable);
	return true;
}

VariablePtr BlockScope::lookup(const std::string &name) const {
	if (variables.count(name) != 0)
		return variables.at(name);
	return parent->lookup(name);
}

FunctionPtr BlockScope::lookupFunction(const std::string &name) const {
	return parent->lookupFunction(name);
}

TypePtr BlockScope::lookupType(const std::string &name) const {
	return parent->lookupType(name);
}

bool BlockScope::doesConflict(const std::string &name) const {
	// It's fine if the parent scope has a conflict because it can be shadowed in this scope.
	return variables.count(name) != 0;
}

bool BlockScope::insert(VariablePtr variable) {
	if (doesConflict(variable->name))
		return false;
	variables.emplace(variable->name, variable);
	return true;
}
