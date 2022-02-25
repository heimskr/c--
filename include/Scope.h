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
	Program *program = nullptr;
	Scope(Program *program_ = nullptr): program(program_) {}
	virtual ~Scope() {}
	virtual VariablePtr lookup(const std::string &) const = 0;
	virtual Function * lookupFunction(const std::string &) const { return nullptr; }
	virtual TypePtr lookupType(const std::string &) const { return nullptr; }
	virtual bool doesConflict(const std::string &) const = 0;
	/** Returns whether the insertion was successful. Insertion can fail due to conflicts. */
	virtual bool insert(VariablePtr) = 0;
	Scope & setProgram(Program &program_) { program = &program_; return *this; }
	virtual Program & getProgram() const {
		if (!program)
			throw std::runtime_error(std::string(typeid(*this).name()) +
				" instance isn't connected to a program instance");
		return *program;
	}
};

using ScopePtr = std::shared_ptr<Scope>;

struct GlobalScope;

struct EmptyScope: Scope, Makeable<EmptyScope> {
	EmptyScope() = default;
	VariablePtr lookup(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
};

struct BasicScope: Scope, Makeable<BasicScope> {
	std::map<std::string, VariablePtr> variables;
	BasicScope() = default;
	VariablePtr lookup(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
};

struct FunctionScope: Scope, Makeable<FunctionScope> {
	Function &function;
	std::shared_ptr<GlobalScope> parent;
	FunctionScope(Function &function_, std::shared_ptr<GlobalScope> parent_);
	VariablePtr lookup(const std::string &) const override;
	Function * lookupFunction(const std::string &) const override;
	TypePtr lookupType(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
};

struct GlobalScope: Scope, Makeable<GlobalScope> {
	Program &program;
	GlobalScope(Program &program_): program(program_) {}
	VariablePtr lookup(const std::string &) const override;
	Function * lookupFunction(const std::string &) const override;
	TypePtr lookupType(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
};

struct MultiScope: Scope, Makeable<MultiScope> {
	std::vector<ScopePtr> scopes;
	MultiScope(const std::vector<ScopePtr> &scopes_): scopes(scopes_) {}
	template <typename... Args>
	MultiScope(Args &&...args): scopes {std::forward<Args>(args)...} {}
	VariablePtr lookup(const std::string &) const override;
	Function * lookupFunction(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
};

struct BlockScope: Scope, Makeable<BlockScope> {
	std::map<std::string, VariablePtr> variables;
	ScopePtr parent;
	BlockScope(ScopePtr parent_): parent(parent_) {}
	VariablePtr lookup(const std::string &) const override;
	Function * lookupFunction(const std::string &) const override;
	TypePtr lookupType(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
	Program & getProgram() const override { return parent->getProgram(); }
};
