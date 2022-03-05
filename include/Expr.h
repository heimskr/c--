#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "ASTNode.h"
#include "Casting.h"
#include "Checkable.h"
#include "Context.h"
#include "DebugData.h"
#include "Errors.h"
#include "fixed_string.h"
#include "Function.h"
#include "Global.h"
#include "Lexer.h"
#include "Makeable.h"
#include "Program.h"
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
	DebugData debug;
	Expr(const ASTLocation &location_ = {}): debug(location_) {}
	Expr(const DebugData &debug_): debug(debug_) {}
	virtual ~Expr() {}
	virtual Expr * copy() const = 0;
	virtual void compile(VregPtr destination, Function &, ScopePtr, ssize_t multiplier = 1);
	virtual operator std::string() const { return "???"; }
	virtual bool shouldParenthesize() const { return false; }
	virtual size_t getSize(const Context &) const { return 0; } // in bytes
	/** Attempts to evaluate the expression at compile time. */
	virtual std::optional<ssize_t> evaluate(const Context &) const { return std::nullopt; }
	/** This function both performs type checking and returns a type. */
	virtual std::unique_ptr<Type> getType(const Context &) const = 0;
	virtual bool compileAddress(VregPtr, Function &, ScopePtr) { return false; }
	virtual bool isLvalue() const { return false; }
	bool isUnsigned(const Context &context) const { return getType(context)->isUnsigned(); }
	Expr * setLocation(const ASTLocation &location_) { debug.location = location_; return this; }
	const ASTLocation & getLocation() const { return debug.location; }
	Expr * setFunction(const Function &);
	Expr * setDebug(const DebugData &);

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

using Argument = std::variant<Expr *, VregPtr>;
void compileCall(VregPtr, Function &, const Context &, FunctionPtr, const std::vector<Argument> &, const ASTLocation &,
                 size_t = 1);

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

	Expr * copy() const override { return (new BinaryExpr<O>(left->copy(), right->copy()))->setDebug(debug); }

	operator std::string() const override {
		return stringify(left.get()) + " " + std::string(O) + " " + stringify(right.get());
	}

	bool shouldParenthesize() const override { return true; }

	std::unique_ptr<Type> getType(const Context &context) const override {
		if (auto fnptr = getOperator(context))
			return std::unique_ptr<Type>(fnptr->returnType->copy());
		auto left_type = left->getType(context), right_type = right->getType(context);
		if (!(*left_type && *right_type) || !(*right_type && *left_type))
			throw ImplicitConversionError(*left_type, *right_type, getLocation());
		return left_type;
	}

	virtual FunctionPtr getOperator(const Context &context) const {
		TypePtr left_type = left->getType(context), right_type = right->getType(context);
		if (left_type->isStruct())
			left_type = PointerType::make(left_type->copy());
		if (right_type->isStruct())
			right_type = PointerType::make(right_type->copy());
		return context.program->getOperator({left_type.get(), right_type.get()}, operator_str_map.at(std::string(O)),
			getLocation());
	}
};

template <fixstr::fixed_string O>
struct LogicExpr: BinaryExpr<O> {
	using BinaryExpr<O>::BinaryExpr;

	size_t getSize(const Context &context) const override { return this->left->getSize(context); }

	std::unique_ptr<Type> getType(const Context &context) const override {
		if (auto fnptr = this->getOperator(context))
			return std::unique_ptr<Type>(fnptr->returnType->copy());
		auto left_type = this->left->getType(context), right_type = this->right->getType(context);
		auto bool_type = std::make_unique<BoolType>();
		if (!(*left_type && *bool_type))
			throw ImplicitConversionError(*left_type, *bool_type, this->getLocation());
		if (!(*right_type && *bool_type))
			throw ImplicitConversionError(*right_type, *bool_type, this->getLocation());
		return bool_type;
	}
};

template <fixstr::fixed_string O, template <typename T> typename CompFn, typename RS, typename RU = RS>
struct CompExpr: BinaryExpr<O> {
	using BinaryExpr<O>::BinaryExpr;

	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
		if (multiplier != 1)
			throw GenericError(this->getLocation(), "Cannot multiply in CompExpr");
		Context context(function.program, scope);
		if (auto fnptr = this->getOperator(context)) {
			auto left_ptr = structToPointer(*this->left, context);
			auto right_ptr = structToPointer(*this->right, context);
			compileCall(destination, function, context, fnptr, {left_ptr.get(), right_ptr.get()}, this->getLocation(),
				multiplier);
		} else {
			VregPtr temp_var = function.newVar();
			this->left->compile(destination, function, scope, 1);
			this->right->compile(temp_var, function, scope, multiplier);
			if (this->left->getType(context)->isUnsigned())
				function.add<RU>(destination, temp_var, destination)->setDebug(*this);
			else
				function.add<RS>(destination, temp_var, destination)->setDebug(*this);
		}
	}

	size_t getSize(const Context &) const override { return 1; }

	std::optional<ssize_t> evaluate(const Context &context) const override {
		auto left_value  = this->left?  this->left->evaluate(context)  : std::nullopt,
		     right_value = this->right? this->right->evaluate(context) : std::nullopt;
		// TODO: signedness
		if (left_value && right_value)
			return CompFn()(*left_value, *right_value)? 1 : 0;
		return std::nullopt;
	}

	std::unique_ptr<Type> getType(const Context &context) const override {
		if (auto fnptr = this->getOperator(context))
			return std::unique_ptr<Type>(fnptr->returnType->copy());
		auto left_type = this->left->getType(context), right_type = this->right->getType(context);
		if (!(*left_type && *right_type) || !(*right_type && *left_type))
			throw ImplicitConversionError(*left_type, *right_type, this->getLocation());
		return std::make_unique<BoolType>();
	}
};

struct SlRInstruction;
struct SleRInstruction;
struct SgRInstruction;
struct SgeRInstruction;
struct SeqRInstruction;
struct SneqRInstruction;

struct LtExpr:   CompExpr<"<",  std::less,            SlRInstruction,  SluRInstruction> { using CompExpr::CompExpr; };
struct LteExpr:  CompExpr<"<=", std::less_equal,     SleRInstruction, SleuRInstruction> { using CompExpr::CompExpr; };
struct GtExpr:   CompExpr<">",  std::greater,         SgRInstruction,  SguRInstruction> { using CompExpr::CompExpr; };
struct GteExpr:  CompExpr<">=", std::greater_equal,  SgeRInstruction, SgeuRInstruction> { using CompExpr::CompExpr; };
struct EqExpr:   CompExpr<"==", std::greater_equal,  SeqRInstruction> { using CompExpr::CompExpr; };
struct NeqExpr:  CompExpr<"!=", std::greater_equal, SneqRInstruction> { using CompExpr::CompExpr; };

struct PlusExpr: BinaryExpr<"+"> {
	using BinaryExpr::BinaryExpr;

	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override { return 8; }
	std::optional<ssize_t> evaluate(const Context &) const override;
	std::unique_ptr<Type> getType(const Context &context) const override;
};

struct MinusExpr: BinaryExpr<"-"> {
	using BinaryExpr::BinaryExpr;

	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override { return 8; }

	std::optional<ssize_t> evaluate(const Context &context) const override {
		auto left_value  = left?  left->evaluate(context)  : std::nullopt,
		     right_value = right? right->evaluate(context) : std::nullopt;
		if (left_value && right_value)
			return *left_value - *right_value;
		return std::nullopt;
	}

	std::unique_ptr<Type> getType(const Context &context) const override;
};

struct MultExpr: BinaryExpr<"*"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override { return 8; }
	std::optional<ssize_t> evaluate(const Context &context) const override {
		auto left_value  = left?  left->evaluate(context)  : std::nullopt,
		     right_value = right? right->evaluate(context) : std::nullopt;
		if (left_value && right_value)
			return *left_value * *right_value;
		return std::nullopt;
	}
};

struct ShiftLeftExpr: BinaryExpr<"<<"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct ShiftRightExpr: BinaryExpr<">>"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct AndExpr: BinaryExpr<"&"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct OrExpr: BinaryExpr<"|"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct XorExpr: BinaryExpr<"^"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct LandExpr: LogicExpr<"&&"> {
	using LogicExpr::LogicExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct LorExpr: LogicExpr<"||"> {
	using LogicExpr::LogicExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct LxorExpr: LogicExpr<"^^"> {
	using LogicExpr::LogicExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct DivExpr: BinaryExpr<"/"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct ModExpr: BinaryExpr<"%"> {
	using BinaryExpr::BinaryExpr;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override;
	std::optional<ssize_t> evaluate(const Context &) const override;
};

struct NumberExpr: AtomicExpr {
	std::string literal;
	NumberExpr(const std::string &literal_): literal(literal_) {}
	NumberExpr(ssize_t value_): literal(std::to_string(value_) + "s64") {}
	Expr * copy() const override { return (new NumberExpr(literal))->setDebug(debug); }
	operator std::string() const override { return literal; }
	ssize_t getValue() const override;
	std::optional<ssize_t> evaluate(const Context &) const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override;
	size_t getSize() const;
	std::unique_ptr<Type> getType(const Context &) const override;
};

struct BoolExpr: AtomicExpr {
	bool value;
	BoolExpr(bool value_): value(value_) {}
	Expr * copy() const override { return (new BoolExpr(value))->setDebug(debug); }
	operator std::string() const override { return value? "true" : "false"; }
	ssize_t getValue() const override { return value? 1 : 0; }
	std::optional<ssize_t> evaluate(const Context &) const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::unique_ptr<Type> getType(const Context &) const override { return std::make_unique<BoolType>(); }
};

struct NullExpr: AtomicExpr {
	NullExpr() {}
	Expr * copy() const override { return (new NullExpr)->setDebug(debug); }
	operator std::string() const override { return "null"; }
	ssize_t getValue() const override { return 0; }
	size_t getSize(const Context &) const override { return 8; }
	std::optional<ssize_t> evaluate(const Context &) const override { return getValue(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	std::unique_ptr<Type> getType(const Context &) const override {
		return std::make_unique<PointerType>(new VoidType);
	}
};

struct VregExpr: Expr, Makeable<VregExpr> {
	VregPtr virtualRegister;
	VregExpr(VregPtr virtual_register): virtualRegister(virtual_register) {}
	Expr * copy() const override { return (new VregExpr(virtualRegister))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return virtualRegister->regOrID(); }
	size_t getSize(const Context &) const override;
	std::unique_ptr<Type> getType(const Context &) const override;
};

struct VariableExpr: Expr {
	std::string name;
	VariableExpr(const std::string &name_): name(name_) {}
	Expr * copy() const override { return (new VariableExpr(name))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return name; }
	size_t getSize(const Context &) const override;
	std::unique_ptr<Type> getType(const Context &) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	bool isLvalue() const override { return true; }
};

struct AddressOfExpr: Expr, Makeable<AddressOfExpr> {
	std::unique_ptr<Expr> subexpr;
	AddressOfExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	AddressOfExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return (new AddressOfExpr(subexpr->copy()))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "&" + std::string(*subexpr); }
	size_t getSize(const Context &) const override { return 8; }
	std::unique_ptr<Type> getType(const Context &) const override;
};

struct NotExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	NotExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	NotExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return (new NotExpr(subexpr->copy()))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "~" + std::string(*subexpr); }
	size_t getSize(const Context &context) const override { return subexpr->getSize(context); }
	std::unique_ptr<Type> getType(const Context &context) const override;
};

struct LnotExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	LnotExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	LnotExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return (new LnotExpr(subexpr->copy()))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "!" + std::string(*subexpr); }
	size_t getSize(const Context &context) const override { return subexpr->getSize(context); }
	std::unique_ptr<Type> getType(const Context &context) const override;
};

struct StringExpr: Expr {
	std::string contents;
	StringExpr(const std::string &contents_): contents(contents_) {}
	Expr * copy() const override { return (new StringExpr(contents))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "\"" + Util::escape(contents) + "\""; }
	size_t getSize(const Context &) const override { return 8; }
	std::unique_ptr<Type> getType(const Context &) const override;

	private:
		std::string getID(Program &) const;
};

struct DerefExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	DerefExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	DerefExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return (new DerefExpr(subexpr->copy()))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "*" + std::string(*subexpr); }
	size_t getSize(const Context &) const override;
	std::unique_ptr<Type> getType(const Context &) const override;
	std::unique_ptr<Type> checkType(const Context &) const;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	bool isLvalue() const override { return true; }
	FunctionPtr getOperator(const Context &) const;
};

struct CallExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	/** This will be null unless the call is for a non-static struct method. */
	std::unique_ptr<Expr> structExpr;
	/** This will be empty unless the call is for a static struct method. */
	std::string structName;
	std::vector<ExprPtr> arguments;
	/** Takes ownership of the subexpr argument. */
	CallExpr(Expr *subexpr_, const std::vector<ExprPtr> &arguments_ = {}):
		subexpr(subexpr_), arguments(arguments_) {}
	/** Takes ownership of the subexpr argument. */
	CallExpr(Expr *subexpr_, std::vector<ExprPtr> &&arguments_):
		subexpr(subexpr_), arguments(std::move(arguments_)) {}
	Expr * copy() const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override;
	size_t getSize(const Context &) const override;
	std::unique_ptr<Type> getType(const Context &) const override;
	FunctionPtr findFunction(const std::string &name, const Context &) const;
	std::string getStructName(const Context &) const;
	FunctionPtr getOperator(const Context &) const;
};

struct AssignExpr: BinaryExpr<"="> {
	using BinaryExpr::BinaryExpr;
	bool shouldParenthesize() const override { return false; }
	std::unique_ptr<Type> getType(const Context &context) const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &context) const override { return left->getSize(context); }
	std::optional<ssize_t> evaluate(const Context &) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	bool isLvalue() const override { return true; }
};

template <fixstr::fixed_string O, typename RS, typename FnS, typename RU = RS, typename FnU = FnS>
struct CompoundAssignExpr: BinaryExpr<O> {
	using BinaryExpr<O>::BinaryExpr;
	Expr * copy() const override {
		return (new CompoundAssignExpr<O, RS, FnS, RU, FnU>(this->left->copy(), this->right->copy()))
			->setDebug(this->debug);
	}
	std::unique_ptr<Type> getType(const Context &context) const override {
		if (auto fnptr = this->getOperator(context))
			return std::unique_ptr<Type>(fnptr->returnType->copy());
		auto left_type = this->left->getType(context), right_type = this->right->getType(context);
		if (!(*right_type && *left_type))
				throw ImplicitConversionError(*right_type, *left_type, this->getLocation());
		return left_type;
	}
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
		Context context(function.program, scope);
		TypePtr left_type = this->left->getType(context);
		if (left_type->isConst)
			throw ConstError("Can't assign", *left_type, this->getLocation());
		auto right_type = this->right->getType(context);
		if (auto fnptr = this->getOperator(context)) {
			auto left_ptr = structToPointer(*this->left, context);
			auto right_ptr = structToPointer(*this->right, context);
			compileCall(destination, function, context, fnptr, {left_ptr.get(), right_ptr.get()}, this->getLocation(),
				multiplier);
		} else {
			auto temp_var = function.newVar();
			if (!destination)
				destination = function.newVar();
			this->left->compile(destination, function, scope);
			this->right->compile(temp_var, function, scope);
			if (this->left->getType(context)->isUnsigned())
				function.add<RU>(destination, temp_var, destination)->setDebug(*this);
			else
				function.add<RS>(destination, temp_var, destination)->setDebug(*this);
			TypePtr right_type = this->right->getType(context);
			if (!tryCast(*right_type, *left_type, destination, function, this->getLocation()))
				throw ImplicitConversionError(right_type, left_type, this->getLocation());
			if (!this->left->compileAddress(temp_var, function, scope))
				throw LvalueError(*this->left->getType(context), this->getLocation());
			function.add<StoreRInstruction>(destination, temp_var, getSize(context))->setDebug(*this);
			if (multiplier != 1)
				function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
		}
	}
	size_t getSize(const Context &context) const override {
		return this->left->getSize(context);
	}
	std::optional<ssize_t> evaluate(const Context &context) const override {
		auto left_value  = this->left?  this->left->evaluate(context)  : std::nullopt,
		     right_value = this->right? this->right->evaluate(context) : std::nullopt;
		if (left_value && right_value) {
			if (this->left->getType(context)->isUnsigned())
				return FnU()(*left_value, *right_value);
			return FnS()(*left_value, *right_value);
		}
		return std::nullopt;
	}
	bool compileAddress(VregPtr destination, Function &function, ScopePtr scope) override {
		return this->left? this->left->compileAddress(destination, function, scope) : false;
	}
	bool isLvalue() const override { return bool(this->left); }
};

struct Add  { ssize_t operator()(ssize_t left, ssize_t right) const { return left +  right; } };
struct Sub  { ssize_t operator()(ssize_t left, ssize_t right) const { return left -  right; } };
struct Mult { ssize_t operator()(ssize_t left, ssize_t right) const { return left *  right; } };
struct Div  { ssize_t operator()(ssize_t left, ssize_t right) const { return left /  right; } };
struct DivU { ssize_t operator()(size_t  left, size_t  right) const { return left /  right; } };
struct Mod  { ssize_t operator()(ssize_t left, ssize_t right) const { return left %  right; } };
struct ModU { ssize_t operator()(size_t  left, size_t  right) const { return left %  right; } };
struct ShL  { ssize_t operator()(ssize_t left, ssize_t right) const { return left << right; } };
struct ShR  { ssize_t operator()(ssize_t left, ssize_t right) const { return left >> right; } };
struct ShRU { ssize_t operator()(size_t  left, size_t  right) const { return left >> right; } };
struct And  { ssize_t operator()(ssize_t left, ssize_t right) const { return left &  right; } };
struct Or   { ssize_t operator()(ssize_t left, ssize_t right) const { return left |  right; } };
struct Xor  { ssize_t operator()(ssize_t left, ssize_t right) const { return left ^  right; } };

struct MultAssignExpr: CompoundAssignExpr<"*=", MultRInstruction, Mult> {
	using CompoundAssignExpr::CompoundAssignExpr;
};
struct DivAssignExpr: CompoundAssignExpr<"/=", DivRInstruction, Div, DivuRInstruction, DivU> {
	using CompoundAssignExpr::CompoundAssignExpr;
};
struct ModAssignExpr: CompoundAssignExpr<"%=", ModRInstruction, Mod, ModuRInstruction, ModU> {
	using CompoundAssignExpr::CompoundAssignExpr;
};
struct AndAssignExpr: CompoundAssignExpr<"&=", AndRInstruction, And> {
	using CompoundAssignExpr::CompoundAssignExpr;
};
struct OrAssignExpr: CompoundAssignExpr<"|=", OrRInstruction, Or>  {
	using CompoundAssignExpr::CompoundAssignExpr;
};
struct XorAssignExpr: CompoundAssignExpr<"^=", XorRInstruction, Xor> {
	using CompoundAssignExpr::CompoundAssignExpr;
};
struct ShiftLeftAssignExpr: CompoundAssignExpr<"<<=", ShiftLeftLogicalRInstruction, ShL> {
	using CompoundAssignExpr::CompoundAssignExpr;
};
struct ShiftRightAssignExpr:
CompoundAssignExpr<">>=", ShiftRightArithmeticRInstruction, ShR, ShiftRightLogicalRInstruction, ShRU> {
	using CompoundAssignExpr::CompoundAssignExpr;
};

inline ExprPtr ensurePointer(const Expr &expr, const Context &context) {
	auto type = expr.getType(context);
	if (type->isPointer())
		return ExprPtr(expr.copy());
	auto out = AddressOfExpr::make(expr.copy());
	out->setDebug(expr.debug);
	return out;
}

inline ExprPtr structToPointer(const Expr &expr, const Context &context) {
	auto type = expr.getType(context);
	if (!type->isStruct())
		return ExprPtr(expr.copy());
	auto out = AddressOfExpr::make(expr.copy());
	out->setDebug(expr.debug);
	return out;
}

template <fixstr::fixed_string O, typename R, typename Fn>
struct PointerArithmeticAssignExpr: CompoundAssignExpr<O, R, Fn> {
	using CompoundAssignExpr<O, R, Fn>::CompoundAssignExpr;
	Expr * copy() const override {
		return (new PointerArithmeticAssignExpr<O, R, Fn>(this->left->copy(), this->right->copy()))
			->setDebug(this->debug);
	}
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
		Context context(function.program, scope);
		TypePtr left_type = this->left->getType(context);
		if (left_type->isConst)
			throw ConstError("Can't assign", *left_type, this->getLocation());
		TypePtr right_type = this->right->getType(context);
		if (auto fnptr = this->getOperator(context)) {
			auto pointer = AddressOfExpr::make(this->left->copy());
			pointer->setDebug(this->left->getLocation());
			compileCall(destination, function, context, fnptr, {pointer.get(), this->right.get()}, this->getLocation(),
			            multiplier);
		} else {
			auto temp_var = function.newVar();
			if (!destination)
				destination = function.newVar();
			function.doPointerArithmetic(left_type, right_type, *this->left, *this->right, destination, temp_var, scope,
				this->getLocation());
			function.add<R>(destination, temp_var, destination)->setDebug(*this);
			if (!this->left->compileAddress(temp_var, function, scope))
				throw LvalueError(*this->left->getType(context), this->getLocation());
			function.add<StoreRInstruction>(destination, temp_var, this->getSize(context))->setDebug(*this);
			if (multiplier != 1)
				function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
		}
	}
};

struct PlusAssignExpr:  PointerArithmeticAssignExpr<"+=", AddRInstruction, Add> {
	using PointerArithmeticAssignExpr::PointerArithmeticAssignExpr;
};

struct MinusAssignExpr: PointerArithmeticAssignExpr<"-=", SubRInstruction, Sub> {
	using PointerArithmeticAssignExpr::PointerArithmeticAssignExpr;
};

struct CastExpr: Expr {
	std::unique_ptr<Type> targetType;
	std::unique_ptr<Expr> subexpr;
	CastExpr(std::unique_ptr<Type> &&target_type, std::unique_ptr<Expr> &&subexpr_):
		targetType(std::move(target_type)), subexpr(std::move(subexpr_)) {}
	CastExpr(Type *target_type, Expr *subexpr_): targetType(target_type), subexpr(subexpr_) {}
	Expr * copy() const override { return (new CastExpr(targetType->copy(), subexpr->copy()))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "(" + std::string(*targetType) + ") " + std::string(*subexpr); }
	size_t getSize(const Context &) const override { return targetType->getSize(); }
	std::unique_ptr<Type> getType(const Context &) const override;
};

struct AccessExpr: Expr {
	std::unique_ptr<Expr> array, subscript;
	AccessExpr(std::unique_ptr<Expr> &&array_, std::unique_ptr<Expr> &&subscript_):
		array(std::move(array_)), subscript(std::move(subscript_)) {}
	AccessExpr(Expr *array_, Expr *subscript_): array(array_), subscript(subscript_) {}
	Expr * copy() const override { return (new AccessExpr(array->copy(), subscript->copy()))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return std::string(*array) + "[" + std::string(*subscript) + "]"; }
	size_t getSize(const Context &context) const override { return getType(context)->getSize(); }
	std::unique_ptr<Type> getType(const Context &) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	bool isLvalue() const override { return true; } // TODO: verify
	std::unique_ptr<Type> check(const Context &);
	FunctionPtr getOperator(const Context &) const;

	private:
		bool warned = false;
		void fail() const;
};

struct LengthExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	LengthExpr(std::unique_ptr<Expr> &&subexpr_): Expr(subexpr_->debug), subexpr(std::move(subexpr_)) {}
	LengthExpr(Expr *subexpr_): Expr(subexpr_->debug), subexpr(subexpr_) {}
	Expr * copy() const override { return (new LengthExpr(subexpr->copy()))->setDebug(debug); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override { return "#" + std::string(*subexpr); }
	size_t getSize(const Context &context) const override { return subexpr->getSize(context); }
	std::unique_ptr<Type> getType(const Context &context) const override {
		auto type = subexpr->getType(context);
		if (auto fnptr = context.program->getOperator({type.get()}, CPMTOK_HASH, getLocation()))
			return std::unique_ptr<Type>(fnptr->returnType->copy());
		return std::make_unique<UnsignedType>(64);
	}
};

template <fixstr::fixed_string O, typename I>
struct PrefixExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	PrefixExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	PrefixExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return (new PrefixExpr<O, I>(subexpr->copy()))->setDebug(debug); }
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
		Context context(function.program, scope);
		if (auto fnptr = getOperator(context)) {
			auto sub_ptr = structToPointer(*this->subexpr, context);
			compileCall(destination, function, context, fnptr, {sub_ptr.get()}, getLocation(), multiplier);
		} else {
			TypePtr subtype = subexpr->getType(context);
			if (subtype->isConst)
				throw ConstError("Can't modify", *subtype, getLocation());
			size_t to_add = 1;
			if (!subtype->isInt()) {
				if (subtype->isPointer())
					to_add = dynamic_cast<PointerType &>(*subtype).subtype->getSize();
				else
					throw GenericError(getLocation(), "Cannot prefix increment/decrement " + std::string(*subtype));
			}
			if (!destination)
				destination = function.newVar();
			auto addr_variable = function.newVar();
			const auto size = subexpr->getSize(context);
			subexpr->compile(function.newVar(), function, scope, multiplier);
			if (!subexpr->compileAddress(addr_variable, function, scope))
				throw LvalueError(*subexpr->getType(context));
			function.addComment("Prefix operator" + std::string(O));
			function.add<LoadRInstruction>(addr_variable, destination, size)->setDebug(*this);
			function.add<I>(destination, destination, to_add)->setDebug(*this);
			function.add<StoreRInstruction>(destination, addr_variable, size)->setDebug(*this);
			if (multiplier != 1)
				function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
		}
	}
	FunctionPtr getOperator(const Context &context) const {
		TypePtr sub_type = this->subexpr->getType(context);
		if (sub_type->isStruct())
			sub_type = PointerType::make(sub_type->copy());
		return context.program->getOperator({sub_type.get()}, operator_str_map.at(std::string(O) + "."), getLocation());
	}
	bool compileAddress(VregPtr destination, Function &function, ScopePtr scope) override {
		return subexpr->compileAddress(destination, function, scope);
	}
	bool isLvalue() const override { return true; }
	operator std::string() const override { return std::string(O) + std::string(*subexpr); }
	size_t getSize(const Context &context) const override { return subexpr->getSize(context); }
	std::unique_ptr<Type> getType(const Context &context) const override {
		if (auto fnptr = getOperator(context))
			return std::unique_ptr<Type>(fnptr->returnType->copy());
		return subexpr->getType(context);
	}
};

struct PrefixPlusExpr:  PrefixExpr<"++", AddIInstruction> { using PrefixExpr::PrefixExpr; };
struct PrefixMinusExpr: PrefixExpr<"--", SubIInstruction> { using PrefixExpr::PrefixExpr; };

template <fixstr::fixed_string O, typename I>
struct PostfixExpr: Expr {
	std::unique_ptr<Expr> subexpr;
	PostfixExpr(std::unique_ptr<Expr> &&subexpr_): subexpr(std::move(subexpr_)) {}
	PostfixExpr(Expr *subexpr_): subexpr(subexpr_) {}
	Expr * copy() const override { return (new PostfixExpr<O, I>(subexpr->copy()))->setDebug(debug); }
	void compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) override {
		Context context(function.program, scope);
		if (auto fnptr = getOperator(context)) {
			auto sub_ptr = structToPointer(*this->subexpr, context);
			compileCall(destination, function, context, fnptr, {sub_ptr.get()}, getLocation(), multiplier);
		} else {
			TypePtr subtype = subexpr->getType(context);
			if (subtype->isConst)
				throw ConstError("Can't modify", *subtype, getLocation());
			size_t to_add = 1;
			if (!subtype->isInt()) {
				if (subtype->isPointer())
					to_add = dynamic_cast<PointerType &>(*subtype).subtype->getSize();
				else
					throw GenericError(getLocation(), "Cannot postfix increment/decrement " + std::string(*subtype));
			}
			if (!destination)
				destination = function.newVar();
			auto temp_var = function.newVar(), addr_var = function.newVar();
			const auto size = subexpr->getSize(context);
			subexpr->compile(function.newVar(), function, scope, multiplier);
			if (!subexpr->compileAddress(addr_var, function, scope))
				throw LvalueError(*subexpr->getType(context));
			function.addComment("Postfix operator" + std::string(O));
			function.add<LoadRInstruction>(addr_var, destination, size)->setDebug(*this);
			function.add<I>(destination, temp_var, to_add)->setDebug(*this);
			function.add<StoreRInstruction>(temp_var, addr_var, size)->setDebug(*this);
			if (multiplier != 1)
				function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
		}
	}
	FunctionPtr getOperator(const Context &context) const {
		TypePtr sub_type = this->subexpr->getType(context);
		if (sub_type->isStruct())
			sub_type = PointerType::make(sub_type->copy());
		return context.program->getOperator({sub_type.get()}, operator_str_map.at("." + std::string(O)), getLocation());
	}
	operator std::string() const override { return std::string(*subexpr) + std::string(O); }
	size_t getSize(const Context &context) const override { return subexpr->getSize(context); }
	std::unique_ptr<Type> getType(const Context &context) const override {
		if (auto fnptr = getOperator(context))
			return std::unique_ptr<Type>(fnptr->returnType->copy());
		return subexpr->getType(context);
	}
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

	Expr * copy() const override {
		return (new TernaryExpr(condition->copy(), ifTrue->copy(), ifFalse->copy()))->setDebug(debug);
	}

	operator std::string() const override {
		return stringify(condition.get()) + "? " + stringify(ifTrue.get()) + " : " + stringify(ifFalse.get());
	}

	bool shouldParenthesize() const override { return true; }

	std::unique_ptr<Type> getType(const Context &context) const override;
	size_t getSize(const Context &) const override;
};

struct DotExpr: Expr {
	std::unique_ptr<Expr> left;
	std::string ident;

	DotExpr(std::unique_ptr<Expr> &&left_, const std::string &ident_): left(std::move(left_)), ident(ident_) {}
	DotExpr(Expr *left_, const std::string &ident_): left(left_), ident(ident_) {}
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	Expr * copy() const override { return (new DotExpr(left->copy(), ident))->setDebug(debug); }
	operator std::string() const override { return stringify(left.get()) + "." + ident; }
	bool shouldParenthesize() const override { return true; }
	std::unique_ptr<Type> getType(const Context &) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	bool isLvalue() const override { return true; }
	std::shared_ptr<StructType> checkType(const Context &) const;
	size_t getSize(const Context &) const override;
};

struct ArrowExpr: Expr {
	std::unique_ptr<Expr> left;
	std::string ident;

	ArrowExpr(std::unique_ptr<Expr> &&left_, const std::string &ident_): left(std::move(left_)), ident(ident_) {}
	ArrowExpr(Expr *left_, const std::string &ident_): left(left_), ident(ident_) {}
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	Expr * copy() const override { return (new ArrowExpr(left->copy(), ident))->setDebug(debug); }
	operator std::string() const override { return stringify(left.get()) + "->" + ident; }
	bool shouldParenthesize() const override { return true; }
	std::unique_ptr<Type> getType(const Context &) const override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	bool isLvalue() const override { return true; }
	std::shared_ptr<StructType> checkType(const Context &) const;
	size_t getSize(const Context &) const override;
};

struct SizeofExpr: Expr, Makeable<SizeofExpr> {
	TypePtr argument;
	SizeofExpr(TypePtr argument_): argument(argument_) {}
	Expr * copy() const override { return (new SizeofExpr(argument))->setDebug(debug); }
	operator std::string() const override { return "sizeof(" + std::string(*argument) + ")"; }
	std::optional<ssize_t> evaluate(const Context &) const override { return argument->getSize(); }
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override { return 8; }
	std::unique_ptr<Type> getType(const Context &) const override { return std::make_unique<UnsignedType>(64); }
};

struct OffsetofExpr: Expr {
	std::string structName, fieldName;
	OffsetofExpr(const std::string &struct_name, const std::string &field_name):
		structName(struct_name), fieldName(field_name) {}
	Expr * copy() const override { return (new OffsetofExpr(structName, fieldName))->setDebug(debug); }
	operator std::string() const override { return "offsetof(%" + structName  + ", " + fieldName + ")"; }
	std::optional<ssize_t> evaluate(const Context &) const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override { return 8; }
	std::unique_ptr<Type> getType(const Context &) const override { return std::make_unique<UnsignedType>(64); }
};

struct SizeofMemberExpr: Expr {
	std::string structName, fieldName;
	SizeofMemberExpr(const std::string &struct_name, const std::string &field_name):
		structName(struct_name), fieldName(field_name) {}
	Expr * copy() const override { return (new SizeofMemberExpr(structName, fieldName))->setDebug(debug); }
	operator std::string() const override { return "sizeof(%" + structName  + ", " + fieldName + ")"; }
	std::optional<ssize_t> evaluate(const Context &) const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	size_t getSize(const Context &) const override { return 8; }
	std::unique_ptr<Type> getType(const Context &) const override { return std::make_unique<UnsignedType>(64); }
};

struct InitializerExpr: Expr {
	std::vector<ExprPtr> children;
	bool isConstructor;
	InitializerExpr(std::vector<ExprPtr> &&children_, bool is_constructor):
		children(std::move(children_)), isConstructor(is_constructor) {}
	InitializerExpr(const std::vector<ExprPtr> &children_, bool is_constructor):
		children(children_), isConstructor(is_constructor) {}
	Expr * copy() const override;
	std::unique_ptr<Type> getType(const Context &) const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	void fullCompile(VregPtr start, Function &function, ScopePtr scope);
};

struct ConstructorExpr: Expr {
	size_t stackOffset;
	std::string structName;
	std::vector<ExprPtr> arguments;
	ConstructorExpr(size_t stack_offset, const std::string &struct_name, const std::vector<ExprPtr> &arguments_ = {}):
		stackOffset(stack_offset), structName(struct_name), arguments(arguments_) {}
	ConstructorExpr(size_t stack_offset, const std::string &struct_name, std::vector<ExprPtr> &&arguments_):
		stackOffset(stack_offset), structName(struct_name), arguments(std::move(arguments_)) {}
	Expr * copy() const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override;
	size_t getSize(const Context &) const override;
	std::unique_ptr<Type> getType(const Context &) const override;
	FunctionPtr findFunction(const Context &) const;
	ConstructorExpr * addToScope(const Context &);
};

struct NewExpr: Expr {
	TypePtr type;
	std::vector<ExprPtr> arguments;
	NewExpr(TypePtr type_, const std::vector<ExprPtr> &arguments_ = {}):
		type(type_), arguments(arguments_) {}
	NewExpr(TypePtr type_, std::vector<ExprPtr> &&arguments_):
		type(type_), arguments(std::move(arguments_)) {}
	Expr * copy() const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	operator std::string() const override;
	size_t getSize(const Context &) const override;
	std::unique_ptr<Type> getType(const Context &) const override;
	FunctionPtr findFunction(const Context &) const;
};

struct StaticFieldExpr: Expr {
	std::string structName, fieldName;
	StaticFieldExpr(const std::string &struct_name, const std::string &field_name);
	Expr * copy() const override;
	void compile(VregPtr, Function &, ScopePtr, ssize_t) override;
	bool compileAddress(VregPtr, Function &, ScopePtr) override;
	bool isLvalue() const override { return true; }
	operator std::string() const override;
	size_t getSize(const Context &) const override;
	std::unique_ptr<Type> getType(const Context &) const override;
	std::shared_ptr<StructType> getStruct(ScopePtr) const;
	std::string mangle(const Context &) const;
};
