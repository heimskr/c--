#include "ASTNode.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Global.h"
#include "Lexer.h"
#include "Program.h"
#include "Scope.h"
#include "WhyInstructions.h"

std::string stringify(const Expr *expr) {
	if (!expr)
		return "...";
	if (expr->shouldParenthesize())
		return "(" + std::string(*expr) + ")";
	return std::string(*expr);
}

void Expr::compile(VregPtr, Function &, ScopePtr, ssize_t) const {}

Expr * Expr::get(const ASTNode &node, Function *function) {
	switch (node.symbol) {
		case CMMTOK_PLUS:
			return new PlusExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_MINUS:
			return new MinusExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_TIMES:
			if (node.size() == 1)
				return new DerefExpr(Expr::get(*node.front(), function));
			return new MultExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_NUMBER:
			return new NumberExpr(ssize_t(node.atoi()));
		case CMMTOK_TRUE:
			return new BoolExpr(true);
		case CMMTOK_FALSE:
			return new BoolExpr(false);
		case CMMTOK_AND:
			return new AddressOfExpr(Expr::get(*node.front(), function));
		case CMMTOK_IDENT:
			if (!function)
				throw std::runtime_error("Variable expression encountered in functionless context");
			return new VariableExpr(*node.lexerInfo);
		case CMMTOK_STRING:
			return new StringExpr(node.unquote());
		default:
			return nullptr;
	}
}

void PlusExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	auto left_type = left->getType(scope), right_type = right->getType(scope);

	if (left_type->isPointer() && right_type->isInt()) {
		if (multiplier != 1)
			throw std::invalid_argument("Cannot multiply in pointer arithmetic PlusExpr");
		auto *left_subtype = dynamic_cast<PointerType &>(*left_type).subtype;
		left->compile(left_var, function, scope, 1);
		right->compile(right_var, function, scope, left_subtype->getSize());
	} else if (left_type->isInt() && right_type->isPointer()) {
		if (multiplier != 1)
			throw std::invalid_argument("Cannot multiply in pointer arithmetic PlusExpr");
		auto *right_subtype = dynamic_cast<PointerType &>(*right_type).subtype;
		left->compile(left_var, function, scope, right_subtype->getSize());
		right->compile(right_var, function, scope, 1);
	} else if (!(*left_type && *right_type)) {
		throw ImplicitConversionError(TypePtr(left_type->copy()), TypePtr(right_type->copy()));
	} else {
		left->compile(left_var, function, scope);
		right->compile(right_var, function, scope);
	}

	function.why.emplace_back(new AddRInstruction(left_var, right_var, destination));
}

void MinusExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	auto left_type = left->getType(scope), right_type = right->getType(scope);

	if (left_type->isPointer() && right_type->isInt()) {
		if (multiplier != 1)
			throw std::invalid_argument("Cannot multiply in pointer arithmetic MinusExpr");
		auto *left_subtype = dynamic_cast<PointerType &>(*left_type).subtype;
		left->compile(left_var, function, scope, 1);
		right->compile(right_var, function, scope, left_subtype->getSize());
	} else if (left_type->isInt() && right_type->isPointer()) {
		throw std::runtime_error("Cannot subtract " + std::string(*right_type) + " from " + std::string(*left_type));
	} else if (!(*left_type && *right_type)) {
		throw ImplicitConversionError(TypePtr(left_type->copy()), TypePtr(right_type->copy()));
	} else {
		left->compile(left_var, function, scope);
		right->compile(right_var, function, scope);
	}

	function.why.emplace_back(new AddRInstruction(left_var, right_var, destination));
}

void MultExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	left->compile(left_var, function, scope, 1);
	right->compile(right_var, function, scope, multiplier); // TODO: verify
	function.why.emplace_back(new MultRInstruction(left_var, right_var, destination));
}

void NumberExpr::compile(VregPtr destination, Function &function, ScopePtr, ssize_t multiplier) const {
	const ssize_t multiplied = value * multiplier;
	if (Util::inRange(multiplied)) {
		function.why.emplace_back(new SetIInstruction(destination, int(multiplied)));
	} else {
		const size_t high = size_t(multiplied) >> 32;
		const size_t low  = size_t(multiplied) & 0xff'ff'ff'ff;
		function.why.emplace_back(new SetIInstruction(destination, int(low)));
		function.why.emplace_back(new LuiIInstruction(destination, int(high)));
	}
}

void BoolExpr::compile(VregPtr destination, Function &function, ScopePtr, ssize_t multiplier) const {
	function.why.emplace_back(new SetIInstruction(destination, value? int(multiplier) : 0));
}

void VariableExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	if (VariablePtr var = scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var))
			function.why.emplace_back(new LoadIInstruction(destination, global->name));
		else
			function.why.emplace_back(new MoveInstruction(var, destination));
		if (multiplier != 1)
			function.why.emplace_back(new MultIInstruction(destination, destination, int(multiplier)));
	} else
		throw ResolutionError(name, scope);
}

size_t VariableExpr::getSize(ScopePtr scope) const {
	if (VariablePtr var = scope->lookup(name))
		return var->getSize();
	throw ResolutionError(name, scope);
}

std::unique_ptr<Type> VariableExpr::getType(ScopePtr scope) const {
	if (VariablePtr var = scope->lookup(name))
		return std::unique_ptr<Type>(var->type->copy());
	throw ResolutionError(name, scope);
}

void AddressOfExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	if (multiplier != 1)
		throw std::invalid_argument("Cannot multiply in AddressOfExpr");
	if (auto *var_exp = dynamic_cast<VariableExpr *>(subexpr.get())) {
		if (auto var = scope->lookup(var_exp->name)) {
			if (auto global = std::dynamic_pointer_cast<Global>(var))
				function.why.emplace_back(new SetIInstruction(destination, global->name));
			else
				function.why.emplace_back(new AddIInstruction(function.precolored(Why::framePointerOffset),
					destination, var));
		} else
			throw ResolutionError(var_exp->name, scope);
	} else
		throw LvalueError(*subexpr);
}

std::unique_ptr<Type> AddressOfExpr::getType(ScopePtr scope) const {
	if (auto *var_exp = dynamic_cast<VariableExpr *>(subexpr.get())) {
		auto subtype = var_exp->getType(scope);
		return std::make_unique<PointerType>(subtype->copy());
	} else
		throw LvalueError(*subexpr);
}

void StringExpr::compile(VregPtr destination, Function &function, ScopePtr, ssize_t multiplier) const {
	if (multiplier != 1)
		throw std::invalid_argument("Cannot multiply in StringExpr");
	function.why.emplace_back(new SetIInstruction(destination, std::to_string(function.program.getStringID(contents))));
}

std::unique_ptr<Type> StringExpr::getType(ScopePtr) const {
	return std::make_unique<PointerType>(new UnsignedType(8));
}

void DerefExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	checkType(scope);
	subexpr->compile(destination, function, scope, multiplier);
	function.why.emplace_back(new LoadRInstruction(destination, destination));
}

size_t DerefExpr::getSize(ScopePtr scope) const {
	auto type = checkType(scope);
	return dynamic_cast<PointerType &>(*type).subtype->getSize();
}

std::unique_ptr<Type> DerefExpr::getType(ScopePtr scope) const {
	auto type = checkType(scope);
	return std::unique_ptr<Type>(dynamic_cast<PointerType &>(*type).subtype->copy());
}

std::unique_ptr<Type> DerefExpr::checkType(ScopePtr scope) const {
	auto type = subexpr->getType(scope);
	if (!type->isPointer())
		throw NotPointerError(TypePtr(type->copy()));
	return type;
}
