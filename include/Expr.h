#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "ASTNode.h"
#include "Checkable.h"
#include "Errors.h"
#include "fixed_string.h"
#include "Function.h"
#include "Global.h"
#include "Scope.h"
#include "Type.h"
#include "Util.h"
#include "Variable.h"
#include "WhyInstructions.h"

class Function;
struct Program;
struct Type;
struct WhyInstruction;

using ScopePtr = std::shared_ptr<Scope>;

struct Expr: Checkable, std::enable_shared_from_this<Expr> {
	ASTLocation location;
	Expr(const ASTLocation &location_ = {}): location(location_) {}
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
	virtual bool compileAddress(VregPtr, Function &, ScopePtr) const { return false; }
	virtual bool isUnsigned() const { return false; }
	Expr * setLocation(const ASTLocation &location_) { location = location_; return this; }

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

std::string stringify(const Expr *);

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
		auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
		     right_value = right? right->evaluate(scope) : std::nullopt;
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

struct AndExpr: BinaryExpr<"&"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct OrExpr: BinaryExpr<"&"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct LandExpr: BinaryExpr<"&&"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct LorExpr: BinaryExpr<"||"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct DivExpr: BinaryExpr<"/"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct ModExpr: BinaryExpr<"%"> {
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
	bool compileAddress(VregPtr, Function &, ScopePtr) const override;
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

struct NotExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	NotExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	NotExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new NotExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "~" + std::string(*subexpr); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr scope) const override { return subexpr->getType(scope); }
	std::vector<std::string> references() const override { return subexpr->references(); }
};

struct LnotExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	LnotExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	LnotExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new LnotExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "!" + std::string(*subexpr); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<BoolType>(); }
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
	bool compileAddress(VregPtr, Function &, ScopePtr) const override;

	private:
		std::string getID(Program &) const;
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
	bool compileAddress(VregPtr, Function &, ScopePtr) const override;
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
	bool compileAddress(VregPtr, Function &, ScopePtr) const override;
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

struct AccessExpr: Expr {
	std::unique_ptr<Expr> array, subscript;
	AccessExpr(std::unique_ptr<Expr> &&array_, std::unique_ptr<Expr> &&subscript_):
		array(std::move(array_)), subscript(std::move(subscript_)) {}
	AccessExpr(Expr *array_, Expr *subscript_): array(array_), subscript(subscript_) {}
	Expr * copy() const override { return new AccessExpr(array->copy(), subscript->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return std::string(*array) + "[" + std::string(*subscript) + "]"; }
	size_t getSize(ScopePtr scope) const override { return getType(scope)->getSize(); }
	std::unique_ptr<Type> getType(ScopePtr) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) const override;
	std::unique_ptr<ArrayType> checkType(ScopePtr) const;
};

struct LengthExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	LengthExpr(std::unique_ptr<Expr> &&subexpr_): Expr(subexpr_->location), subexpr(std::move(subexpr_)) {}
	LengthExpr(Expr *subexpr_): Expr(subexpr_->location), subexpr(subexpr_) {}
	Expr * copy() const override { return new LengthExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) const override;
	operator std::string() const override { return "#" + std::string(*subexpr); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<UnsignedType>(64); }
	std::vector<std::string> references() const override { return subexpr->references(); }
};

template <fixstr::fixed_string O, typename I>
struct PrefixExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	PrefixExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	PrefixExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new PrefixExpr<O, I>(subexpr->copy()); }
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const override {
		TypePtr subtype = subexpr->getType(scope);
		if (!subtype->isInt())
			throw std::runtime_error("Cannot increment/decrement " + std::string(*subtype));
		if (!destination)
			destination = function.newVar();
		if (auto *var_expr = subexpr->cast<VariableExpr>()) {
			if (auto var = scope->lookup(var_expr->name)) {
				if (auto *global = var->cast<Global>()) {
					function.add<LoadIInstruction>(destination, global->name, global->getSize());
					function.add<I>(destination, destination, 1);
					function.add<StoreIInstruction>(destination, global->name, global->getSize());
				} else {
					auto fp = function.precolored(Why::framePointerOffset);
					auto offset = function.stackOffsets.at(var);
					VregPtr addr;
					if (offset == 0) {
						addr = fp;
					} else {
						addr = function.newVar();
						function.add<SubIInstruction>(fp, addr, offset);
					}
					function.add<LoadRInstruction>(addr, destination, var->getSize());
					function.add<I>(destination, destination, 1);
					function.add<StoreRInstruction>(destination, addr, var->getSize());
				}
			} else
				throw ResolutionError(var_expr->name, scope);
		} else if (auto *deref_expr = subexpr->cast<DerefExpr>()) {
			deref_expr->checkType(scope);
			auto addr_variable = function.newVar();
			deref_expr->subexpr->compile(addr_variable, function, scope);
			const auto deref_size = deref_expr->getSize(scope);
			function.add<LoadRInstruction>(addr_variable, destination, deref_size);
			function.add<I>(destination, destination, 1);
			function.add<StoreRInstruction>(destination, addr_variable, deref_size);
		} else if (auto *access_expr = subexpr->cast<AccessExpr>()) {
			access_expr->checkType(scope);
			auto addr_variable = function.newVar();
			access_expr->compileAddress(addr_variable, function, scope);
			const auto access_size = access_expr->getSize(scope);
			function.add<LoadRInstruction>(addr_variable, destination, access_size);
			function.add<I>(destination, destination, 1);
			function.add<StoreRInstruction>(destination, addr_variable, access_size);
		} else
			throw LvalueError(*subexpr->getType(scope));

		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, multiplier);
	}
	operator std::string() const override { return std::string(O) + std::string(*subexpr); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr scope) const override { return subexpr->getType(scope); }
	std::vector<std::string> references() const override { return subexpr->references(); }
};

struct PrefixPlusExpr:  PrefixExpr<"++", AddIInstruction> { using PrefixExpr::PrefixExpr; };
struct PrefixMinusExpr: PrefixExpr<"--", SubIInstruction> { using PrefixExpr::PrefixExpr; };

template <fixstr::fixed_string O, typename I>
struct PostfixExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	PostfixExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	PostfixExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new PostfixExpr<O, I>(subexpr->copy()); }
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const override {
		TypePtr subtype = subexpr->getType(scope);
		if (!subtype->isInt())
			throw std::runtime_error("Cannot increment/decrement " + std::string(*subtype));
		if (!destination)
			destination = function.newVar();
		auto temp_var = function.newVar();
		if (auto *var_expr = subexpr->cast<VariableExpr>()) {
			if (auto var = scope->lookup(var_expr->name)) {
				if (auto *global = var->cast<Global>()) {
					function.add<LoadIInstruction>(destination, global->name, global->getSize());
					function.add<I>(destination, temp_var, 1);
					function.add<StoreIInstruction>(temp_var, global->name, global->getSize());
				} else {
					auto fp = function.precolored(Why::framePointerOffset);
					auto offset = function.stackOffsets.at(var);
					VregPtr addr;
					if (offset == 0) {
						addr = fp;
					} else {
						addr = function.newVar();
						function.add<SubIInstruction>(fp, addr, offset);
					}
					function.add<LoadRInstruction>(addr, destination, var->getSize());
					function.add<I>(destination, temp_var, 1);
					function.add<StoreRInstruction>(temp_var, addr, var->getSize());
				}
			} else
				throw ResolutionError(var_expr->name, scope);
		} else if (auto *deref_expr = subexpr->cast<DerefExpr>()) {
			deref_expr->checkType(scope);
			auto addr_variable = function.newVar();
			deref_expr->subexpr->compile(addr_variable, function, scope);
			const auto deref_size = deref_expr->getSize(scope);
			function.add<LoadRInstruction>(addr_variable, destination, deref_size);
			function.add<I>(destination, temp_var, 1);
			function.add<StoreRInstruction>(temp_var, addr_variable, deref_size);
		} else if (auto *access_expr = subexpr->cast<AccessExpr>()) {
			access_expr->checkType(scope);
			auto addr_variable = function.newVar();
			access_expr->compileAddress(addr_variable, function, scope);
			const auto access_size = access_expr->getSize(scope);
			function.add<LoadRInstruction>(addr_variable, destination, access_size);
			function.add<I>(destination, temp_var, 1);
			function.add<StoreRInstruction>(temp_var, addr_variable, access_size);
		} else
			throw LvalueError(*subexpr->getType(scope));

		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, multiplier);
	}
	operator std::string() const override { return std::string(*subexpr) + std::string(O); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr scope) const override { return subexpr->getType(scope); }
	std::vector<std::string> references() const override { return subexpr->references(); }
};

struct PostfixPlusExpr:  PostfixExpr<"++", AddIInstruction> { using PostfixExpr::PostfixExpr; };
struct PostfixMinusExpr: PostfixExpr<"--", SubIInstruction> { using PostfixExpr::PostfixExpr; };
