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
	virtual void compile(VregPtr destination, Function &, ScopePtr, ssize_t multiplier = 1) const;
	virtual operator std::string() const { return "???"; }
	virtual bool shouldParenthesize() const { return false; }
	virtual size_t getSize(ScopePtr) const { return 0; } // in bytes
	/** Attempts to evaluate the expression at compile time. */
	virtual std::optional<ssize_t> evaluate() const { return std::nullopt; }
	/** Returns a vector of all variable names referenced by the expression or its children. */
	virtual std::vector<std::string> references() const { return {}; }
	/** This function both performs type checking and returns a type. */
	virtual std::unique_ptr<Type> getType(ScopePtr) const = 0;
	static Expr * get(const ASTNode &, Function * = nullptr);
};

using ExprPtr = std::shared_ptr<Expr>;

struct AtomicExpr: Expr {
	virtual ssize_t getValue() const = 0;
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

	std::unique_ptr<Type> getType(ScopePtr scope) const override {
		auto left_type = left->getType(scope), right_type = right->getType(scope);
		if (!(*left_type && *right_type) || !(*right_type && *left_type))
			throw ImplicitConversionError(*left_type, *right_type);
		return left_type;
	}
};

// TODO: signedness
template <char O, typename CompFn, typename R>
struct CompExpr: BinaryExpr<O> {
	using BinaryExpr<O>::BinaryExpr;

	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const override {
		if (multiplier != 1)
			throw std::runtime_error("Cannot multiply in CompExpr");
		VregPtr left_var = function.newVar(), right_var = function.newVar();
		this->left->compile(left_var, function, scope, 1);
		this->right->compile(right_var, function, scope, multiplier);
		function.add<R>(left_var, right_var, destination);
	}

	size_t getSize(ScopePtr) const override { return 1; }

	std::optional<ssize_t> evaluate() const override {
		auto left_value  = this->left?  this->left->evaluate()  : std::nullopt,
		     right_value = this->right? this->right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return CompFn()(*left_value, *right_value)? 1 : 0;
		return std::nullopt;
	}

	std::unique_ptr<Type> getType(ScopePtr scope) const override {
		auto left_type = this->left->getType(scope), right_type = this->right->getType(scope);
		if (!(*left_type && *right_type) || !(*right_type && *left_type))
			throw ImplicitConversionError(*left_type, *right_type);
		return std::make_unique<BoolType>();
	}
};

struct PlusExpr: BinaryExpr<'+'> {
	using BinaryExpr::BinaryExpr;

	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override { return 8; }

	std::optional<ssize_t> evaluate() const override {
		auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return *left_value + *right_value;
		return std::nullopt;
	}

	std::unique_ptr<Type> getType(ScopePtr scope) const override {
		auto left_type = left->getType(scope), right_type = right->getType(scope);
		if (left_type->isPointer() && right_type->isInt())
			return left_type;
		if (left_type->isInt() && right_type->isPointer())
			return right_type;
		if (!(*left_type && *right_type) || !(*right_type && *left_type))
			throw ImplicitConversionError(*left_type, *right_type);
		return left_type;
	}
};

struct MinusExpr: BinaryExpr<'-'> {
	using BinaryExpr::BinaryExpr;

	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override { return 8; }

	std::optional<ssize_t> evaluate() const override {
		auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return *left_value - *right_value;
		return std::nullopt;
	}

	std::unique_ptr<Type> getType(ScopePtr scope) const override {
		auto left_type = left->getType(scope), right_type = right->getType(scope);
		if (left_type->isPointer() && right_type->isInt())
			return left_type;
		if (!(*left_type && *right_type) || !(*right_type && *left_type))
			throw ImplicitConversionError(*left_type, *right_type);
		return left_type;
	}
};

struct MultExpr: BinaryExpr<'*'> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override { return 8; }
	std::optional<ssize_t> evaluate() const override {
		auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
		if (left_value && right_value)
			return *left_value * *right_value;
		return std::nullopt;
	}
};

struct NumberExpr: AtomicExpr {
	ssize_t value;
	NumberExpr(ssize_t value_): value(value_) {}
	operator std::string() const override { return std::to_string(value); }
	ssize_t getValue() const override { return value; }
	std::optional<ssize_t> evaluate() const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	// TODO: support unsigned literals and literals of other widths
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<SignedType>(64); }
};

struct BoolExpr: AtomicExpr {
	bool value;
	BoolExpr(bool value_): value(value_) {}
	operator std::string() const override { return value? "true" : "false"; }
	ssize_t getValue() const override { return value? 1 : 0; }
	std::optional<ssize_t> evaluate() const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<BoolType>(); }
};

struct VariableExpr: Expr {
	std::string name;
	VariableExpr(const std::string &name_): name(name_) {}
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return name; }
	size_t getSize(ScopePtr) const override;
	std::vector<std::string> references() const override { return {name}; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
};

struct AddressOfExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	AddressOfExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	AddressOfExpr(Expr *subexpr_): subexpr(subexpr_) {}
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "&" + std::string(*subexpr); }
	size_t getSize(ScopePtr) const override { return 8; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
	std::vector<std::string> references() const override { return subexpr->references(); }
};

struct StringExpr: Expr {
	std::string contents;
	StringExpr(const std::string &contents_): contents(contents_) {}
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "\"" + Util::escape(contents) + "\""; }
	size_t getSize(ScopePtr) const override { return 8; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
};

class DerefExpr: public Expr {
	public:
		std::unique_ptr<Expr> subexpr;
		DerefExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
		DerefExpr(Expr *subexpr_): subexpr(subexpr_) {}
		void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
		operator std::string() const override { return "*" + std::string(*subexpr); }
		size_t getSize(ScopePtr) const override;
		std::unique_ptr<Type> getType(ScopePtr) const override;
		std::vector<std::string> references() const override { return subexpr->references(); }

	private:
		std::unique_ptr<Type> checkType(ScopePtr) const;
};

struct CallExpr: Expr {
	std::string name;
	Function *function;
	std::vector<ExprPtr> arguments;
	CallExpr(const ASTNode &, Function *);
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override;
	size_t getSize(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
};
