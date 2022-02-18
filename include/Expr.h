#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "Checkable.h"
#include "Errors.h"
#include "fixed_string.h"
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

struct Expr: Checkable, std::enable_shared_from_this<Expr> {
	virtual ~Expr() {}
	virtual Expr * copy() const = 0;
	virtual void compile(VregPtr destination, Function &, ScopePtr, ssize_t multiplier = 1) const;
	virtual operator std::string() const { return "???"; }
	virtual bool shouldParenthesize() const { return false; }
	virtual size_t getSize(ScopePtr) const { return 0; } // in bytes
	/** Attempts to evaluate the expression at compile time. */
	virtual std::optional<ssize_t> evaluate(ScopePtr) const { return std::nullopt; }
	/** Returns a vector of all variable names referenced by the expression or its children. */
	virtual std::vector<std::string> references() const { return {}; }
	/** This function both performs type checking and returns a type. */
	virtual std::unique_ptr<Type> getType(ScopePtr) const = 0;
	virtual bool isUnsigned() const { return false; }

	static Expr * get(const ASTNode &, Function * = nullptr);

	template <typename T>
	std::shared_ptr<T> ptrcast() {
		return std::dynamic_pointer_cast<T>(shared_from_this());
	}

	template <typename T>
	std::shared_ptr<const T> ptrcast() const {
		return std::dynamic_pointer_cast<const T>(shared_from_this());
	}
};

std::ostream & operator<<(std::ostream &, const Expr &);

using ExprPtr = std::shared_ptr<Expr>;

struct AtomicExpr: Expr {
	virtual ssize_t getValue() const = 0;
};

template <fixstr::fixed_string O>
struct BinaryExpr: Expr {
	std::unique_ptr<Expr> left, right;

	BinaryExpr(std::unique_ptr<Expr> &&left_, std::unique_ptr<Expr> &&right_):
		left(std::move(left_)), right(std::move(right_)) {}

	BinaryExpr(Expr *left_, Expr *right_): left(left_), right(right_) {}

	Expr * copy() const override { return new BinaryExpr<O>(left->copy(), right->copy()); }

	operator std::string() const override {
		return stringify(left.get()) + " " + std::string(O) + " " + stringify(right.get());
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
template <fixstr::fixed_string O, template <typename T> typename CompFn, typename R>
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

	std::optional<ssize_t> evaluate(ScopePtr scope) const override {
		auto left_value  = this->left?  this->left->evaluate(scope)  : std::nullopt,
		     right_value = this->right? this->right->evaluate(scope) : std::nullopt;
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

struct LtRInstruction;
struct LteRInstruction;
struct GtRInstruction;
struct GteRInstruction;
struct EqRInstruction;
struct NeqRInstruction;

struct LtExpr:   CompExpr<"<",  std::less,           LtRInstruction>  { using CompExpr::CompExpr; };
struct LteExpr:  CompExpr<"<=", std::less_equal,    LteRInstruction> { using CompExpr::CompExpr; };
struct GtExpr:   CompExpr<">",  std::greater,        GtRInstruction>  { using CompExpr::CompExpr; };
struct GteExpr:  CompExpr<">=", std::greater_equal, GteRInstruction> { using CompExpr::CompExpr; };
struct EqExpr:   CompExpr<"==", std::greater_equal,  EqRInstruction> { using CompExpr::CompExpr; };
struct NeqExpr:  CompExpr<"<=", std::greater_equal, NeqRInstruction> { using CompExpr::CompExpr; };

struct PlusExpr: BinaryExpr<"+"> {
	using BinaryExpr::BinaryExpr;

	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override { return 8; }
	std::optional<ssize_t> evaluate(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr scope) const override;
};

struct MinusExpr: BinaryExpr<"-"> {
	using BinaryExpr::BinaryExpr;

	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override { return 8; }

	std::optional<ssize_t> evaluate(ScopePtr scope) const override {
		auto left_value = left? left->evaluate(scope) : std::nullopt, right_value = right? right->evaluate(scope) : std::nullopt;
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

struct MultExpr: BinaryExpr<"*"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override { return 8; }
	std::optional<ssize_t> evaluate(ScopePtr scope) const override {
		auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
		     right_value = right? right->evaluate(scope) : std::nullopt;
		if (left_value && right_value)
			return *left_value * *right_value;
		return std::nullopt;
	}
};

struct ShiftLeftExpr: BinaryExpr<"<<"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct ShiftRightExpr: BinaryExpr<">>"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct NumberExpr: AtomicExpr {
	std::string literal;
	NumberExpr(const std::string &literal_): literal(literal_) {}
	NumberExpr(ssize_t value_): literal(std::to_string(value_) + "s64") {}
	Expr * copy() const override { return new NumberExpr(literal); }
	operator std::string() const override { return literal; }
	ssize_t getValue() const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
	bool isUnsigned() const override;
};

struct BoolExpr: AtomicExpr {
	bool value;
	BoolExpr(bool value_): value(value_) {}
	Expr * copy() const override { return new BoolExpr(value); }
	operator std::string() const override { return value? "true" : "false"; }
	ssize_t getValue() const override { return value? 1 : 0; }
	std::optional<ssize_t> evaluate(ScopePtr) const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<BoolType>(); }
};

struct VariableExpr: Expr {
	std::string name;
	VariableExpr(const std::string &name_): name(name_) {}
	Expr * copy() const override { return new VariableExpr(name); }
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
	Expr * copy() const override { return new AddressOfExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "&" + std::string(*subexpr); }
	size_t getSize(ScopePtr) const override { return 8; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
	std::vector<std::string> references() const override { return subexpr->references(); }
};

struct StringExpr: Expr {
	std::string contents;
	StringExpr(const std::string &contents_): contents(contents_) {}
	Expr * copy() const override { return new StringExpr(contents); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "\"" + Util::escape(contents) + "\""; }
	size_t getSize(ScopePtr) const override { return 8; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
};

struct DerefExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	DerefExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	DerefExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new DerefExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "*" + std::string(*subexpr); }
	size_t getSize(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
	std::vector<std::string> references() const override { return subexpr->references(); }
	std::unique_ptr<Type> checkType(ScopePtr) const;
};

struct CallExpr: Expr {
	std::string name;
	Function *function;
	std::vector<ExprPtr> arguments;
	CallExpr(const ASTNode &, Function *);
	CallExpr(const std::string &name_, Function *function_, const std::vector<ExprPtr> &arguments_):
		name(name_), function(function_), arguments(arguments_) {}
	Expr * copy() const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override;
	size_t getSize(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
};

struct AssignExpr: BinaryExpr<"="> {
	using BinaryExpr::BinaryExpr;
	bool shouldParenthesize() const override { return false; }
	std::unique_ptr<Type> getType(ScopePtr scope) const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct CastExpr: Expr {
	std::unique_ptr<Type> targetType;
	std::unique_ptr<Expr> subexpr;
	CastExpr(std::unique_ptr<Type> &&target_type, std::unique_ptr<Expr> &&subexpr_):
		targetType(std::move(target_type)), subexpr(std::move(subexpr_)) {}
	CastExpr(Type *target_type, Expr *subexpr_): targetType(target_type), subexpr(subexpr_) {}
	Expr * copy() const override { return new CastExpr(targetType->copy(), subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "(" + std::string(*targetType) + ") " + std::string(*subexpr); }
	size_t getSize(ScopePtr) const override { return targetType->getSize(); }
	std::unique_ptr<Type> getType(ScopePtr) const override;
};
