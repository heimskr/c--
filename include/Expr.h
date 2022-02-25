#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "ASTNode.h"
#include "Casting.h"
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
	virtual void compile(VregPtr destination, Function &, ScopePtr, ssize_t multiplier = 1);
	virtual operator std::string() const { return "???"; }
	virtual bool shouldParenthesize() const { return false; }
	virtual size_t getSize(ScopePtr) const { return 0; } // in bytes
	/** Attempts to evaluate the expression at compile time. */
	virtual std::optional<ssize_t> evaluate(ScopePtr) const { return std::nullopt; }
	/** This function both performs type checking and returns a type. */
	virtual std::unique_ptr<Type> getType(ScopePtr) const = 0;
	virtual bool compileAddress(VregPtr, Function &, ScopePtr) { return false; }
	bool isUnsigned(ScopePtr scope) const { return getType(scope)->isUnsigned(); }
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

	std::unique_ptr<Type> getType(ScopePtr scope) const override {
		auto left_type = left->getType(scope), right_type = right->getType(scope);
		if (!(*left_type && *right_type) || !(*right_type && *left_type))
			throw ImplicitConversionError(*left_type, *right_type);
		return left_type;
	}
};

template <fixstr::fixed_string O>
struct LogicExpr: BinaryExpr<O> {
	using BinaryExpr<O>::BinaryExpr;

	size_t getSize(ScopePtr scope) const override { return this->left->getSize(scope); }

	std::unique_ptr<Type> getType(ScopePtr scope) const override {
		auto left_type = this->left->getType(scope), right_type = this->right->getType(scope);
		auto bool_type = std::make_unique<BoolType>();
		if (!(*left_type && *bool_type))
			throw ImplicitConversionError(*left_type, *bool_type);
		if (!(*right_type && *bool_type))
			throw ImplicitConversionError(*right_type, *bool_type);
		return bool_type;
	}
};

// TODO: signedness
template <fixstr::fixed_string O, template <typename T> typename CompFn, typename R>
struct CompExpr: BinaryExpr<O> {
	using BinaryExpr<O>::BinaryExpr;

	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
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

	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override { return 8; }
	std::optional<ssize_t> evaluate(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr scope) const override;
};

struct MinusExpr: BinaryExpr<"-"> {
	using BinaryExpr::BinaryExpr;

	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override { return 8; }

	std::optional<ssize_t> evaluate(ScopePtr scope) const override {
		auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
		     right_value = right? right->evaluate(scope) : std::nullopt;
		if (left_value && right_value)
			return *left_value - *right_value;
		return std::nullopt;
	}

	std::unique_ptr<Type> getType(ScopePtr scope) const override;
};

struct MultExpr: BinaryExpr<"*"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
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
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct ShiftRightExpr: BinaryExpr<">>"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct AndExpr: BinaryExpr<"&"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct OrExpr: BinaryExpr<"&"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct LandExpr: LogicExpr<"&&"> {
	using LogicExpr::LogicExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct LorExpr: LogicExpr<"||"> {
	using LogicExpr::LogicExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct LxorExpr: LogicExpr<"^^"> {
	using LogicExpr::LogicExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct DivExpr: BinaryExpr<"/"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override;
	std::optional<ssize_t> evaluate(ScopePtr) const override;
};

struct ModExpr: BinaryExpr<"%"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
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
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
};

struct BoolExpr: AtomicExpr {
	bool value;
	BoolExpr(bool value_): value(value_) {}
	Expr * copy() const override { return new BoolExpr(value); }
	operator std::string() const override { return value? "true" : "false"; }
	ssize_t getValue() const override { return value? 1 : 0; }
	std::optional<ssize_t> evaluate(ScopePtr) const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<BoolType>(); }
};

struct NullExpr: AtomicExpr {
	NullExpr() {}
	Expr * copy() const override { return new NullExpr(); }
	operator std::string() const override { return "null"; }
	ssize_t getValue() const override { return 0; }
	std::optional<ssize_t> evaluate(ScopePtr) const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<PointerType>(new VoidType); }
};

struct VariableExpr: Expr {
	std::string name;
	VariableExpr(const std::string &name_): name(name_) {}
	Expr * copy() const override { return new VariableExpr(name); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return name; }
	size_t getSize(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
};

struct AddressOfExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	AddressOfExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	AddressOfExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new AddressOfExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "&" + std::string(*subexpr); }
	size_t getSize(ScopePtr) const override { return 8; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
};

struct NotExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	NotExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	NotExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new NotExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "~" + std::string(*subexpr); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr scope) const override { return subexpr->getType(scope); }
};

struct LnotExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	LnotExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	LnotExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new LnotExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "!" + std::string(*subexpr); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<BoolType>(); }
};

struct StringExpr: Expr {
	std::string contents;
	StringExpr(const std::string &contents_): contents(contents_) {}
	Expr * copy() const override { return new StringExpr(contents); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "\"" + Util::escape(contents) + "\""; }
	size_t getSize(ScopePtr) const override { return 8; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;

	private:
		std::string getID(Program &) const;
};

struct DerefExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	DerefExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	DerefExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new DerefExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "*" + std::string(*subexpr); }
	size_t getSize(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
	std::unique_ptr<Type> checkType(ScopePtr) const;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
};

struct CallExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	std::vector<ExprPtr> arguments;
	/** Takes ownership of the subexpr argument. */
	CallExpr(Expr *subexpr_, const std::vector<ExprPtr> &arguments_):
		subexpr(subexpr_), arguments(arguments_) {}
	Expr * copy() const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override;
	size_t getSize(ScopePtr) const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
};

struct AssignExpr: BinaryExpr<"="> {
	using BinaryExpr::BinaryExpr;
	bool shouldParenthesize() const override { return false; }
	std::unique_ptr<Type> getType(ScopePtr scope) const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr scope) const override { return left->getSize(scope); }
	std::optional<ssize_t> evaluate(ScopePtr) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
};

template <fixstr::fixed_string O, typename RS, typename FnS, typename RU = RS, typename FnU = FnS>
struct CompoundAssignExpr: BinaryExpr<O> {
	using BinaryExpr<O>::BinaryExpr;
	std::unique_ptr<Type> getType(ScopePtr scope) const override {
		auto left_type = this->left->getType(scope), right_type = this->right->getType(scope);
		if (!(*right_type && *left_type))
				throw ImplicitConversionError(*right_type, *left_type);
		return left_type;
	}
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
		TypePtr left_type = this->left->getType(scope);
		if (left_type->isConst)
			throw ConstError("Can't assign", *left_type, this->location);
		auto temp_var = function.newVar();
		if (!destination)
			destination = function.newVar();
		this->left->compile(destination, function, scope);
		this->right->compile(temp_var, function, scope);
		if (this->left->getType(scope)->isUnsigned())
			function.add<RU>(destination, temp_var, destination);
		else
			function.add<RS>(destination, temp_var, destination);
		TypePtr right_type = this->right->getType(scope);
		if (!tryCast(*right_type, *left_type, destination, function))
			throw ImplicitConversionError(right_type, left_type);
		if (!this->left->compileAddress(temp_var, function, scope))
			throw LvalueError(*this->left->getType(scope));
		function.add<StoreRInstruction>(destination, temp_var, getSize(scope));
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier));
	}
	size_t getSize(ScopePtr scope) const override {
		return this->left->getSize(scope);
	}
	std::optional<ssize_t> evaluate(ScopePtr scope) const override {
		auto left_value  = this->left?  this->left->evaluate(scope)  : std::nullopt,
		     right_value = this->right? this->right->evaluate(scope) : std::nullopt;
		if (left_value && right_value) {
			if (this->left->getType(scope)->isUnsigned())
				return FnU()(*left_value, *right_value);
			return FnS()(*left_value, *right_value);
		}
		return std::nullopt;
	}
	bool compileAddress(VregPtr destination, Function &function, ScopePtr scope) override {
		return this->left? this->left->compileAddress(destination, function, scope) : false;
	}
};

struct Add  { ssize_t operator()(ssize_t left, ssize_t right) const { return left + right; } };
struct Sub  { ssize_t operator()(ssize_t left, ssize_t right) const { return left - right; } };
struct Mult { ssize_t operator()(ssize_t left, ssize_t right) const { return left * right; } };
struct Div  { ssize_t operator()(ssize_t left, ssize_t right) const { return left / right; } };
struct DivU { ssize_t operator()(size_t  left, size_t  right) const { return left / right; } };
struct Mod  { ssize_t operator()(ssize_t left, ssize_t right) const { return left % right; } };
struct ModU { ssize_t operator()(size_t  left, size_t  right) const { return left % right; } };
struct ShL  { ssize_t operator()(ssize_t left, ssize_t right) const { return left << right; } };
struct ShR  { ssize_t operator()(ssize_t left, ssize_t right) const { return left >> right; } };
struct ShRU { ssize_t operator()(size_t  left, size_t  right) const { return left >> right; } };
struct And  { ssize_t operator()(ssize_t left, ssize_t right) const { return left & right; } };
struct Or   { ssize_t operator()(ssize_t left, ssize_t right) const { return left | right; } };
struct Xor  { ssize_t operator()(ssize_t left, ssize_t right) const { return left ^ right; } };
struct PlusAssignExpr:  CompoundAssignExpr<"+=", AddRInstruction, Add>  { using CompoundAssignExpr::CompoundAssignExpr; };
struct MinusAssignExpr: CompoundAssignExpr<"-=", SubRInstruction, Sub>  { using CompoundAssignExpr::CompoundAssignExpr; };
struct MultAssignExpr:  CompoundAssignExpr<"*=", MultRInstruction, Mult> { using CompoundAssignExpr::CompoundAssignExpr; };
struct DivAssignExpr:   CompoundAssignExpr<"/=", DivRInstruction, Div, DivuRInstruction, DivU> { using CompoundAssignExpr::CompoundAssignExpr; };
struct ModAssignExpr:   CompoundAssignExpr<"%=", ModRInstruction, Mod, ModuRInstruction, ModU> { using CompoundAssignExpr::CompoundAssignExpr; };
struct AndAssignExpr:   CompoundAssignExpr<"&=", AndRInstruction, And> { using CompoundAssignExpr::CompoundAssignExpr; };
struct OrAssignExpr:    CompoundAssignExpr<"|=", OrRInstruction, Or>  { using CompoundAssignExpr::CompoundAssignExpr; };
struct XorAssignExpr:   CompoundAssignExpr<"^=", XorRInstruction, Xor> { using CompoundAssignExpr::CompoundAssignExpr; };
struct ShiftLeftAssignExpr:  CompoundAssignExpr<"<<=", ShiftLeftLogicalRInstruction, ShL> { using CompoundAssignExpr::CompoundAssignExpr; };
struct ShiftRightAssignExpr: CompoundAssignExpr<">>=", ShiftRightArithmeticRInstruction, ShR, ShiftRightLogicalRInstruction, ShRU> { using CompoundAssignExpr::CompoundAssignExpr; };

struct CastExpr: Expr {
	std::unique_ptr<Type> targetType;
	std::unique_ptr<Expr> subexpr;
	CastExpr(std::unique_ptr<Type> &&target_type, std::unique_ptr<Expr> &&subexpr_):
		targetType(std::move(target_type)), subexpr(std::move(subexpr_)) {}
	CastExpr(Type *target_type, Expr *subexpr_): targetType(target_type), subexpr(subexpr_) {}
	Expr * copy() const override { return new CastExpr(targetType->copy(), subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
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
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return std::string(*array) + "[" + std::string(*subscript) + "]"; }
	size_t getSize(ScopePtr scope) const override { return getType(scope)->getSize(); }
	std::unique_ptr<Type> getType(ScopePtr) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	std::unique_ptr<Type> check(ScopePtr);

	private:
		bool warned = false;
};

struct LengthExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	LengthExpr(std::unique_ptr<Expr> &&subexpr_): Expr(subexpr_->location), subexpr(std::move(subexpr_)) {}
	LengthExpr(Expr *subexpr_): Expr(subexpr_->location), subexpr(subexpr_) {}
	Expr * copy() const override { return new LengthExpr(subexpr->copy()); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "#" + std::string(*subexpr); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<UnsignedType>(64); }
};

template <fixstr::fixed_string O, typename I>
struct PrefixExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	PrefixExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	PrefixExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new PrefixExpr<O, I>(subexpr->copy()); }
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
		TypePtr subtype = subexpr->getType(scope);
		if (subtype->isConst)
			throw ConstError("Can't modify", *subtype, location);
		size_t to_add = 1;
		if (!subtype->isInt()) {
			if (subtype->isPointer())
				to_add = dynamic_cast<PointerType &>(*subtype).subtype->getSize();
			else
				throw std::runtime_error("Cannot increment/decrement " + std::string(*subtype));
		}
		if (!destination)
			destination = function.newVar();
		auto addr_variable = function.newVar();
		const auto size = subexpr->getSize(scope);
		subexpr->compile(function.newVar(), function, scope, multiplier);
		if (!subexpr->compileAddress(addr_variable, function, scope))
			throw LvalueError(*subexpr->getType(scope));
		function.addComment("Prefix operator" + std::string(O));
		function.add<LoadRInstruction>(addr_variable, destination, size);
		function.add<I>(destination, destination, to_add);
		function.add<StoreRInstruction>(destination, addr_variable, size);

		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier));
	}
	bool compileAddress(VregPtr destination, Function &function, ScopePtr scope) override {
		return subexpr->compileAddress(destination, function, scope);
	}
	operator std::string() const override { return std::string(O) + std::string(*subexpr); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr scope) const override { return subexpr->getType(scope); }
};

struct PrefixPlusExpr:  PrefixExpr<"++", AddIInstruction> { using PrefixExpr::PrefixExpr; };
struct PrefixMinusExpr: PrefixExpr<"--", SubIInstruction> { using PrefixExpr::PrefixExpr; };

template <fixstr::fixed_string O, typename I>
struct PostfixExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	PostfixExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	PostfixExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return new PostfixExpr<O, I>(subexpr->copy()); }
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
		TypePtr subtype = subexpr->getType(scope);
		if (subtype->isConst)
			throw ConstError("Can't modify", *subtype, location);
		size_t to_add = 1;
		if (!subtype->isInt()) {
			if (subtype->isPointer())
				to_add = dynamic_cast<PointerType &>(*subtype).subtype->getSize();
			else
				throw std::runtime_error("Cannot increment/decrement " + std::string(*subtype));
		}
		if (!destination)
			destination = function.newVar();
		auto temp_var = function.newVar(), addr_var = function.newVar();
		const auto size = subexpr->getSize(scope);
		subexpr->compile(function.newVar(), function, scope, multiplier);
		if (!subexpr->compileAddress(addr_var, function, scope))
			throw LvalueError(*subexpr->getType(scope));
		function.addComment("Postfix operator" + std::string(O));
		function.add<LoadRInstruction>(addr_var, destination, size);
		function.add<I>(destination, temp_var, to_add);
		function.add<StoreRInstruction>(temp_var, addr_var, size);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier));
	}
	operator std::string() const override { return std::string(*subexpr) + std::string(O); }
	size_t getSize(ScopePtr scope) const override { return subexpr->getSize(scope); }
	std::unique_ptr<Type> getType(ScopePtr scope) const override { return subexpr->getType(scope); }
};

struct PostfixPlusExpr:  PostfixExpr<"++", AddIInstruction> { using PostfixExpr::PostfixExpr; };
struct PostfixMinusExpr: PostfixExpr<"--", SubIInstruction> { using PostfixExpr::PostfixExpr; };

struct TernaryExpr: Expr {
	std::unique_ptr<Expr> condition, ifTrue, ifFalse;

	TernaryExpr(std::unique_ptr<Expr> &&condition_, std::unique_ptr<Expr> &&if_true, std::unique_ptr<Expr> &&if_false):
		condition(std::move(condition_)), ifTrue(std::move(if_true)), ifFalse(std::move(if_false)) {}

	TernaryExpr(Expr *condition_, Expr *if_true, Expr *if_false):
		condition(condition_), ifTrue(if_true), ifFalse(if_false) {}

	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;

	Expr * copy() const override { return new TernaryExpr(condition->copy(), ifTrue->copy(), ifFalse->copy()); }

	operator std::string() const override {
		return stringify(condition.get()) + "? " + stringify(ifTrue.get()) + " : " + stringify(ifFalse.get());
	}

	bool shouldParenthesize() const override { return true; }

	std::unique_ptr<Type> getType(ScopePtr scope) const override;
	size_t getSize(ScopePtr) const override;
};

struct DotExpr: Expr {
	std::unique_ptr<Expr> left;
	std::string ident;

	DotExpr(std::unique_ptr<Expr> &&left_, const std::string &ident_): left(std::move(left_)), ident(ident_) {}
	DotExpr(Expr *left_, const std::string &ident_): left(left_), ident(ident_) {}
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	Expr * copy() const override { return new DotExpr(left->copy(), ident); }
	operator std::string() const override { return stringify(left.get()) + "." + ident; }
	bool shouldParenthesize() const override { return true; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	std::shared_ptr<StructType> checkType(ScopePtr) const;
	size_t getSize(ScopePtr) const override;
};

struct ArrowExpr: Expr {
	std::unique_ptr<Expr> left;
	std::string ident;

	ArrowExpr(std::unique_ptr<Expr> &&left_, const std::string &ident_): left(std::move(left_)), ident(ident_) {}
	ArrowExpr(Expr *left_, const std::string &ident_): left(left_), ident(ident_) {}
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	Expr * copy() const override { return new ArrowExpr(left->copy(), ident); }
	operator std::string() const override { return stringify(left.get()) + "." + ident; }
	bool shouldParenthesize() const override { return true; }
	std::unique_ptr<Type> getType(ScopePtr) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	std::shared_ptr<StructType> checkType(ScopePtr) const;
	size_t getSize(ScopePtr) const override;
};

struct SizeofExpr: Expr {
	TypePtr argument;
	SizeofExpr(TypePtr argument_): argument(argument_) {}
	Expr * copy() const override { return new SizeofExpr(argument); }
	operator std::string() const override { return "sizeof(" + std::string(*argument) + ")"; }
	std::optional<ssize_t> evaluate(ScopePtr) const override { return argument->getSize(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(ScopePtr) const override { return 8; }
	std::unique_ptr<Type> getType(ScopePtr) const override { return std::make_unique<UnsignedType>(64); }
};

struct InitializerExpr: Expr {
	std::vector<ExprPtr> children;
	InitializerExpr(std::vector<ExprPtr> &&children_): children(std::move(children_)) {}
	InitializerExpr(const std::vector<ExprPtr> &children_): children(children_) {}
	Expr * copy() const override;
	std::unique_ptr<Type> getType(ScopePtr) const override;
};
