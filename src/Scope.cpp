#include "Function.h"
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
	return function.variables.at(name).shared_from_this();
}

VariablePtr MultiScope::lookup(const std::string &name) const {
	for (const auto &scope: scopes)
		if (VariablePtr var = scope->lookup(name))
			return var;
	return nullptr;
}
