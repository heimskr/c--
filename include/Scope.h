#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ASTNode.h"
#include "Checkable.h"
#include "Makeable.h"
#include "Variable.h"

class Function;
struct Program;
struct Type;

using TypePtr = std::shared_ptr<Type>;
using FunctionPtr = std::shared_ptr<Function>;
using Functions = std::vector<FunctionPtr>;
using Types = std::vector<TypePtr>;

struct Scope: Checkable {
	Program *program = nullptr;
	Scope(Program *program_ = nullptr): program(program_) {}
	virtual ~Scope() {}
	virtual VariablePtr lookup(const std::string &) const = 0;

	virtual Functions lookupFunctions(const std::string &function_name, TypePtr, const Types &,
		const std::string &struct_name) const { (void) function_name; (void) struct_name; return {}; }
	FunctionPtr lookupFunction(const std::string &function_name, TypePtr, const Types &, const std::string &struct_name,
		const ASTLocation & = {}) const;

	virtual Functions lookupFunctions(const std::string &function_name, const Types &,
		const std::string &struct_name) const { (void) function_name; (void) struct_name; return {}; }
	FunctionPtr lookupFunction(const std::string &function_name, const Types &, const std::string &struct_name,
		const ASTLocation & = {}) const;

	virtual Functions lookupFunctions(const std::string &) const { return {}; }
	FunctionPtr lookupFunction(const std::string &, const ASTLocation & = {}) const;

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

	virtual Function & getFunction() const { throw std::runtime_error("Scope can't return a function"); }

	virtual std::string partialStringify() const = 0;
	virtual operator std::string() const { return partialStringify(); }
};

using ScopePtr = std::shared_ptr<Scope>;

struct GlobalScope: Scope, Makeable<GlobalScope> {
	Program &program;
	GlobalScope(Program &program_): program(program_) {}
	VariablePtr lookup(const std::string &) const override;
	Functions lookupFunctions(const std::string &, TypePtr, const Types &, const std::string &) const override;
	Functions lookupFunctions(const std::string &, const Types &, const std::string &) const override;
	Functions lookupFunctions(const std::string &) const override;
	TypePtr lookupType(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
	std::string partialStringify() const override { return "global"; }
};

struct FunctionScope: Scope, Makeable<FunctionScope> {
	Function &function;
	std::shared_ptr<GlobalScope> parent;
	FunctionScope(Function &function_, std::shared_ptr<GlobalScope> parent_);
	VariablePtr lookup(const std::string &) const override;
	Functions lookupFunctions(const std::string &, TypePtr, const Types &, const std::string &) const override;
	Functions lookupFunctions(const std::string &, const Types &, const std::string &) const override;
	Functions lookupFunctions(const std::string &) const override;
	TypePtr lookupType(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
	std::string partialStringify() const override;
	Function & getFunction() const override { return function; }
};

struct BlockScope: Scope, Makeable<BlockScope> {
	std::map<std::string, VariablePtr> variables;
	std::vector<VariablePtr> variableOrder;
	ScopePtr parent;
	BlockScope(ScopePtr parent_): parent(parent_) {}
	VariablePtr lookup(const std::string &) const override;
	Functions lookupFunctions(const std::string &, TypePtr, const Types &, const std::string &) const override;
	Functions lookupFunctions(const std::string &, const Types &, const std::string &) const override;
	Functions lookupFunctions(const std::string &) const override;
	TypePtr lookupType(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
	Program & getProgram() const override { return parent->getProgram(); }
	Function & getFunction() const override { return parent->getFunction(); }
	std::string partialStringify() const override { return parent->partialStringify() + " -> block"; }
	operator std::string() const override;
};
