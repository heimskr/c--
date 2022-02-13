#pragma once

#include <memory>
#include <string>
#include <vector>

class ASTNode;
struct Expr;
struct WhyInstruction;

std::string stringify(const Expr *);

struct Expr {
	virtual ~Expr() {}
	virtual void compile(std::vector<std::shared_ptr<WhyInstruction>> &) {}
	virtual operator std::string() const { return "???"; }
	static Expr * get(const ASTNode &);
};

struct AtomicExpr: Expr {};

struct BinaryExpr: Expr {
	std::unique_ptr<Expr> left, right;

	BinaryExpr(std::unique_ptr<Expr> &&left_, std::unique_ptr<Expr> &&right_):
		left(std::move(left_)), right(std::move(right_)) {}

	BinaryExpr(Expr *left_, Expr *right_): left(left_), right(right_) {}
};

struct PlusExpr: BinaryExpr {
	using BinaryExpr::BinaryExpr;
	operator std::string() const override { return stringify(left.get()) + " + " + stringify(right.get()); }
};

struct NumberExpr: AtomicExpr {
	ssize_t value;
	NumberExpr(ssize_t value_): value(value_) {}
	operator std::string() const override { return std::to_string(value); }
};

using ExprPtr = std::shared_ptr<Expr>;
