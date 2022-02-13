#pragma once

#include <memory>
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
	virtual void compile(VariablePtr, Function &) const {}
	virtual operator std::string() const { return "???"; }
	virtual bool shouldParenthesize() const { return false; }
	static Expr * get(const ASTNode &);
};

struct AtomicExpr: Expr {};

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

struct PlusExpr:  BinaryExpr<'+'> {
	using BinaryExpr::BinaryExpr;
	void compile(VariablePtr, Function &) const override;
};

struct MinusExpr: BinaryExpr<'-'> { using BinaryExpr::BinaryExpr; };
struct MultExpr:  BinaryExpr<'*'> { using BinaryExpr::BinaryExpr; };

struct NumberExpr: AtomicExpr {
	ssize_t value;
	NumberExpr(ssize_t value_): value(value_) {}
	operator std::string() const override { return std::to_string(value); }
};

struct BoolExpr: AtomicExpr {
	bool value;
	BoolExpr(bool value_): value(value_) {}
	operator std::string() const override { return value? "true" : "false"; }
};

using ExprPtr = std::shared_ptr<Expr>;
