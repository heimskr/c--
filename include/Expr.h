#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Errors.h"
#include "Function.h"
#include "Type.h"
#include "Util.h"
#include "Variable.h"

class ASTNode;
class Function;
struct Expr;
struct Scope;
struct Type;
struct WhyInstruction;

using ScopePtr = std::shared_ptr<Scope>;

std::string stringify(const Expr *);

struct Expr {
	virtual ~Expr() {}
	virtual void compile(VregPtr, Function &, ScopePtr) const {}
	virtual operator std::string() const { return "???"; }
	virtual bool shouldParenthesize() const { return false; }
	virtual size_t getSize(ScopePtr) const { return 0; } // in bytes
	static Expr * get(const ASTNode &, Function * = nullptr);
	/** Attempts to evaluate the expression at compile time. */
	virtual std::optional<ssize_t> evaluate() const = 0;
	/** Returns a vector of all variable names referenced by the expression or its children. */
	virtual std::vector<std::string> references() const { return {}; }
	/** This function both performs type checking and returns a type. */
	virtual std::unique_ptr<Type> getType() const { return nullptr; }
};

using ExprPtr = std::shared_ptr<Expr>;

struct AtomicExpr: Expr {
	virtual int getValue() const = 0;
};

template <char O>
struct BinaryExpr: Expr {
	std::unique_ptr<Expr> left, right;

	BinaryExpr(std::unique_ptr<Expr> &&left_, std::unique_ptr<Expr> &&right_):
		left(std::move(left_)), right(std::move(right_)) {}

	BinaryExpr(Expr *left_, Expr *right_): left(left_), right(right_) {}

	operator std::string() const override {
		return stringify(left.get()) + " " + std::string(1, O) + " " + stringify(right.get());
	}

	bool shouldParenthesize() const override { return true; }
	std::vector<std::string> references() const override {
		return Util::combine(left? left->references() : std::vector<std::string>(),
			right? right->references() : std::vector<std::string>());
	}

	virtual std::unique_ptr<Type> getType() const override {
		auto left_type = left->getType(), right_type = right->getType();
		if (!(*left_type && *right_type) || !(*right_type && *left_type))
			throw ImplicitConversionError(*left_type, *right_type);
		return left_type;
	}
};

struct PlusExpr: BinaryExpr<'+'> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr) const override;
	size_t getSize(ScopePtr) const override { return 8; }
	std::optional<ssize_t> evaluate() const override {
		auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return *left_value + *right_value;
		return std::nullopt;
	}
};

struct MinusExpr: BinaryExpr<'-'> {
	using BinaryExpr::BinaryExpr;
	size_t getSize(ScopePtr) const override { return 8; }
	std::optional<ssize_t> evaluate() const override {
		auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return *left_value - *right_value;
		return std::nullopt;
	}
};

struct MultExpr: BinaryExpr<'*'> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr) const override;
	size_t getSize(ScopePtr) const override { return 8; }
	std::optional<ssize_t> evaluate() const override {
		auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return *left_value * *right_value;
		return std::nullopt;
	}
};

struct NumberExpr: AtomicExpr {
	int value;
	NumberExpr(int value_): value(value_) {}
	operator std::string() const override { return std::to_string(value); }
	int getValue() const override { return value; }
	std::optional<ssize_t> evaluate() const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr) const override;
};

struct BoolExpr: AtomicExpr {
	bool value;
	BoolExpr(bool value_): value(value_) {}
	operator std::string() const override { return value? "true" : "false"; }
	int getValue() const override { return value? 1 : 0; }
	std::optional<ssize_t> evaluate() const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr) const override;
};

struct VariableExpr: Expr {
	std::string name;
	VariableExpr(const std::string &name_): name(name_) {}
	void compile(VregPtr, Function &, ScopePtr) const override;
	operator std::string() const override { return name; }
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate() const override { return std::nullopt; }
	std::vector<std::string> references() const override { return {name}; }
};

struct AddressOfExpr: Expr {
	std::unique_ptr<Expr> subexpr;

	AddressOfExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	AddressOfExpr(Expr *subexpr_): subexpr(subexpr_) {}

	void compile(VregPtr, Function &, ScopePtr) const override;
	operator std::string() const override { return "&" + std::string(*subexpr); }
	size_t getSize(ScopePtr) const override { return 8; }
	std::optional<ssize_t> evaluate() const override { return std::nullopt; }
	std::vector<std::string> references() const override { return subexpr->references(); }
};
