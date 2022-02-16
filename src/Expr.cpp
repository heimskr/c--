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
				throw std::runtime_error("Variable expr encountered in functionless context");
			return new VariableExpr(*node.lexerInfo);
		case CMMTOK_STRING:
			return new StringExpr(node.unquote());
		default:
			return nullptr;
	}
}

void PlusExpr::compile(VregPtr destination, Function &function, ScopePtr scope) const {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	left->compile(left_var, function, scope);
	right->compile(right_var, function, scope);
	function.why.emplace_back(new AddRInstruction(left_var, right_var, destination));
}

void MultExpr::compile(VregPtr destination, Function &function, ScopePtr scope) const {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	left->compile(left_var, function, scope);
	right->compile(right_var, function, scope);
	function.why.emplace_back(new MultRInstruction(left_var, right_var, destination));
}

void NumberExpr::compile(VregPtr destination, Function &function, ScopePtr) const {
	if (Util::inRange(value)) {
		function.why.emplace_back(new SetIInstruction(destination, int(value)));
	} else {
		const size_t high = size_t(value) >> 32;
		const size_t low = size_t(value) & 0xff'ff'ff'ff;
		function.why.emplace_back(new SetIInstruction(destination, int(low)));
		function.why.emplace_back(new LuiIInstruction(destination, int(high)));
	}
}

void BoolExpr::compile(VregPtr destination, Function &function, ScopePtr) const {
	function.why.emplace_back(new SetIInstruction(destination, value? 1 : 0));
}

void VariableExpr::compile(VregPtr destination, Function &function, ScopePtr scope) const {
	if (VariablePtr var = scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var))
			function.why.emplace_back(new LoadIInstruction(destination, global->name));
		else
			function.why.emplace_back(new MoveInstruction(var, destination));
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

void AddressOfExpr::compile(VregPtr destination, Function &function, ScopePtr scope) const {
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

void StringExpr::compile(VregPtr destination, Function &function, ScopePtr) const {
	function.why.emplace_back(new SetIInstruction(destination, std::to_string(function.program.getStringID(contents))));
}

std::unique_ptr<Type> StringExpr::getType(ScopePtr) const {
	return std::make_unique<PointerType>(new UnsignedType(8));
}
