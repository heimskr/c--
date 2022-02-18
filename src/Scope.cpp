#include "Function.h"
#include "Program.h"
#include "Scope.h"

VariablePtr EmptyScope::lookup(const std::string &) const {
	return nullptr;
}

bool EmptyScope::doesConflict(const std::string &) const {
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

VariablePtr FunctionScope::lookup(const std::string &name) const {
	if (function.variables.count(name) == 0)
		return parent->lookup(name);
	return function.variables.at(name);
}

Function * FunctionScope::lookupFunction(const std::string &name) const {
	return parent->lookupFunction(name);
}

bool FunctionScope::doesConflict(const std::string &name) const {
	// It's fine for local variables to shadow global variables.
	return function.variables.count(name) != 0;
}

VariablePtr GlobalScope::lookup(const std::string &name) const {
	if (program.globals.count(name) == 0)
		return nullptr;
	return program.globals.at(name);
}

Function * GlobalScope::lookupFunction(const std::string &name) const {
	if (program.functions.count(name) == 0)
		return nullptr;
	return &program.functions.at(name);
}

bool GlobalScope::doesConflict(const std::string &name) const {
	return program.globals.count(name) != 0;
}

VariablePtr MultiScope::lookup(const std::string &name) const {
	for (const auto &scope: scopes)
		if (auto var = scope->lookup(name))
			return var;
	return nullptr;
}

Function * MultiScope::lookupFunction(const std::string &name) const {
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

VariablePtr BlockScope::lookup(const std::string &name) const {
	if (variables.count(name) != 0)
		return variables.at(name);
	return parent->lookup(name);
}

Function * BlockScope::lookupFunction(const std::string &name) const {
	return parent->lookupFunction(name);
}

bool BlockScope::doesConflict(const std::string &name) const {
	// It's fine if the parent scope has a conflict because it can be shadowed in this scope.
	return variables.count(name) != 0;
}
