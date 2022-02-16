#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Variable.h"

class Function;

struct Scope {
	virtual ~Scope() {}
	virtual VariablePtr lookup(const std::string &) const = 0;
};

using ScopePtr = std::shared_ptr<Scope>;

struct EmptyScope: Scope {
	EmptyScope() = default;
	VariablePtr lookup(const std::string &) const override;
};

struct BasicScope: Scope {
	std::map<std::string, VariablePtr> variables;
	BasicScope() = default;
	VariablePtr lookup(const std::string &) const override;
};

struct FunctionScope: Scope {
	Function &function;
	FunctionScope(Function &function_): function(function_) {}
	VariablePtr lookup(const std::string &) const override;
};

struct MultiScope: Scope {
	std::vector<ScopePtr> scopes;
	MultiScope(const std::vector<ScopePtr> &scopes_): scopes(scopes_) {}
	VariablePtr lookup(const std::string &) const override;
};

