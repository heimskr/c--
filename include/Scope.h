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

struct Scope: Checkable, std::enable_shared_from_this<Scope> {
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
		if (program == nullptr)
			throw std::runtime_error(std::string(typeid(*this).name()) +
				" instance isn't connected to a program instance");
		return *program;
	}

	virtual Function & getFunction() const { throw std::runtime_error("Scope can't return a function"); }

	virtual std::string getName() const { return ""; }

	virtual std::string partialStringify() const = 0;
	virtual explicit operator std::string() const { return partialStringify(); }

	template <typename T>
	std::shared_ptr<T> ptrcast() {
		return std::dynamic_pointer_cast<T>(shared_from_this());
	}

	template <typename T>
	std::shared_ptr<const T> ptrcast() const {
		return std::dynamic_pointer_cast<const T>(shared_from_this());
	}
} __attribute__((packed));

using ScopePtr = std::shared_ptr<Scope>;

struct GlobalScope: Scope, Makeable<GlobalScope> {
	Program &program;
	explicit GlobalScope(Program &program_): program(program_) {}
	VariablePtr lookup(const std::string &) const override;
	Functions lookupFunctions(const std::string &, TypePtr, const Types &, const std::string &) const override;
	Functions lookupFunctions(const std::string &, const Types &, const std::string &) const override;
	Functions lookupFunctions(const std::string &) const override;
	TypePtr lookupType(const std::string &) const override;
	bool doesConflict(const std::string &) const override;
	bool insert(VariablePtr) override;
	std::string partialStringify() const override { return "global"; }
} __attribute__((packed));

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
	std::string getName() const override { return function.name; }
};

struct BlockScope: Scope, Makeable<BlockScope> {
	std::map<std::string, VariablePtr> variables;
	std::vector<VariablePtr> variableOrder;
	ScopePtr parent;
	std::string name;
	BlockScope(ScopePtr parent_, const std::string &name_ = ""): parent(parent_), name(name_) {}
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
	std::string getName() const override { return name; }
	operator std::string() const override;
};
