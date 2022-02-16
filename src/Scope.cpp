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

VariablePtr MultiScope::lookup(const std::string &name) const {
	for (const auto &scope: scopes)
		if (VariablePtr var = scope->lookup(name))
			return var;
	return nullptr;
}
