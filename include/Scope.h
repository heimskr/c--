#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Makeable.h"
#include "Variable.h"

class Function;
struct Program;

struct Scope {
	virtual ~Scope() {}
	virtual VariablePtr lookup(const std::string &) const = 0;
	virtual Function * lookupFunction(const std::string &) const { return nullptr; }
};

using ScopePtr = std::shared_ptr<Scope>;

struct EmptyScope: Scope, Makeable<EmptyScope> {
	EmptyScope() = default;
	VariablePtr lookup(const std::string &) const override;
};

struct BasicScope: Scope, Makeable<BasicScope> {
	std::map<std::string, VariablePtr> variables;
	BasicScope() = default;
	VariablePtr lookup(const std::string &) const override;
};

struct FunctionScope: Scope, Makeable<FunctionScope> {
	Function &function;
	FunctionScope(Function &function_): function(function_) {}
	VariablePtr lookup(const std::string &) const override;
};

struct GlobalScope: Scope, Makeable<GlobalScope> {
	Program &program;
	GlobalScope(Program &program_): program(program_) {}
	VariablePtr lookup(const std::string &) const override;
	Function * lookupFunction(const std::string &) const override;
};

struct MultiScope: Scope, Makeable<MultiScope> {
	std::vector<ScopePtr> scopes;
	MultiScope(const std::vector<ScopePtr> &scopes_): scopes(scopes_) {}
	template <typename... Args>
	MultiScope(Args &&...args): scopes {std::forward<Args>(args)...} {}
	VariablePtr lookup(const std::string &) const override;
	Function * lookupFunction(const std::string &) const override;
};

