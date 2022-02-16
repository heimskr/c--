#include "Function.h"
#include "Program.h"
#include "Scope.h"

VariablePtr EmptyScope::lookup(const std::string &) const {
	return nullptr;
}

VariablePtr BasicScope::lookup(const std::string &name) const {
	if (variables.count(name) == 0)
		return nullptr;
	return variables.at(name);
}

VariablePtr FunctionScope::lookup(const std::string &name) const {
	if (function.variables.count(name) == 0)
		return nullptr;
	return function.variables.at(name);
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
