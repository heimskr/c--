#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Function.h"

class ASTNode;
class Function;
struct Expr;
struct WhyInstruction;

std::string stringify(const Expr *);

struct Expr {
	virtual ~Expr() {}
	virtual void compile(VregPtr, Function &) const {}
	virtual operator std::string() const { return "???"; }
	virtual bool shouldParenthesize() const { return false; }
	virtual size_t getSize() const { return 0; } // in bytes
	static Expr * get(const ASTNode &);
	/** Attempts to evaluate the expression at compile time. */
	virtual std::optional<ssize_t> evaluate() const = 0;
};

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
};

struct PlusExpr: BinaryExpr<'+'> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &) const override;
	size_t getSize() const override { return 8; }
	std::optional<ssize_t> evaluate() const override {
		auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return *left_value + *right_value;
		return std::nullopt;
	}
};

struct MinusExpr: BinaryExpr<'-'> {
	using BinaryExpr::BinaryExpr;
	std::optional<ssize_t> evaluate() const override {
		auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return *left_value - *right_value;
		return std::nullopt;
	}
};

struct MultExpr: BinaryExpr<'*'> {
	using BinaryExpr::BinaryExpr;
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
};

struct BoolExpr: AtomicExpr {
	bool value;
	BoolExpr(bool value_): value(value_) {}
	operator std::string() const override { return value? "true" : "false"; }
	int getValue() const override { return value? 1 : 0; }
	std::optional<ssize_t> evaluate() const override { return getValue(); }
};

using ExprPtr = std::shared_ptr<Expr>;
