#include <climits>
#include <iostream>
#include <sstream>

#include "ASTNode.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Global.h"
#include "Lexer.h"
#include "Parser.h"
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
			return new NumberExpr(*node.lexerInfo);
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
		case CMMTOK_LPAREN:
			if (!function)
				throw std::runtime_error("Function call expression encountered in functionless context");
			return new CallExpr(node, function);
		case CMMTOK_STRING:
			return new StringExpr(node.unquote());
		case CMMTOK_CHAR:
			return new NumberExpr(std::to_string(ssize_t(node.getChar())) + "_u8");
		case CMMTOK_LT:
			return new LtExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_LTE:
			return new LteExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_GT:
			return new GtExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_GTE:
			return new GteExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_DEQ:
			return new EqExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_NEQ:
			return new NeqExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		case CMMTOK_ASSIGN:
			return new AssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
		default:
			throw std::invalid_argument("Unrecognized symbol in Expr::get: " +
				std::string(cmmParser.getName(node.symbol)));
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

	function.add<AddRInstruction>(left_var, right_var, destination);
}

std::optional<ssize_t> PlusExpr::evaluate() const {
	auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
	if (left_value && right_value)
		return *left_value + *right_value;
	return std::nullopt;
}

std::unique_ptr<Type> PlusExpr::getType(ScopePtr scope) const {
	auto left_type = left->getType(scope), right_type = right->getType(scope);
	if (left_type->isPointer() && right_type->isInt())
		return left_type;
	if (left_type->isInt() && right_type->isPointer())
		return right_type;
	if (!(*left_type && *right_type) || !(*right_type && *left_type))
		throw ImplicitConversionError(*left_type, *right_type);
	return left_type;
}

// Pointer PlusExpr::getPointer() const {
// 	auto left_type = left->getType(scope), right_type = right->getType(scope);

// 	if (left_type->isPointer() && right_type->isInt()) {
// 		return left->getPointer() + right->
// 	}

// 	return {};
// }

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

	function.add<SubRInstruction>(left_var, right_var, destination);
}

void MultExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	left->compile(left_var, function, scope, 1);
	right->compile(right_var, function, scope, multiplier); // TODO: verify
	function.add<MultRInstruction>(left_var, right_var, destination);
}

void ShiftLeftExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	
}

size_t ShiftLeftExpr::getSize(ScopePtr scope) const {

}

std::optional<ssize_t> ShiftLeftExpr::evaluate() const {
	auto left_value = left? left->evaluate() : std::nullopt, right_value = right? right->evaluate() : std::nullopt;
	if (left_value && right_value)
		return *left_value << *right_value;
	return std::nullopt;
}

ssize_t NumberExpr::getValue() const {
	getSize(nullptr);
	return Util::parseLong(literal.substr(0, literal.find('_')));
}

void NumberExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	const ssize_t multiplied = getValue() * multiplier;
	getSize(scope);
	if (Util::inRange(multiplied)) {
		function.add<SetIInstruction>(destination, int(multiplied));
	} else {
		const size_t high = size_t(multiplied) >> 32;
		const size_t low  = size_t(multiplied) & 0xff'ff'ff'ff;
		function.add<SetIInstruction>(destination, int(low));
		function.add<LuiIInstruction>(destination, int(high));
	}
}

size_t NumberExpr::getSize(ScopePtr) const {
	const size_t underscore = literal.find('_');

	// TODO: bounds checking for unsigned literals

	if (underscore == std::string::npos)
		return 8;

	const ssize_t value = Util::parseLong(literal.substr(0, underscore));
	const size_t size = Util::parseLong(literal.substr(underscore + 2)) / 8;
	bool out_of_range = false;

	switch (size) {
		case 8:
			break;
		case 4:
			out_of_range = value < INT_MIN || INT_MAX < value;
			break;
		case 2:
			out_of_range = value < -65536 || 65535 < value;
			break;
		case 1:
			out_of_range = value < -256 || 255 < value;
			break;
		default:
			throw std::invalid_argument("Invalid numeric literal size (in bytes): " + std::to_string(size));
	}

	if (out_of_range)
		throw std::out_of_range("Numeric literal cannot fit in " + std::to_string(size) + " byte" +
			(size == 1? "" : "s") + ": " + std::to_string(value));

	return size;
}

std::unique_ptr<Type> NumberExpr::getType(ScopePtr scope) const {
	const size_t bits = getSize(scope) * 8;
	if (isUnsigned())
		return std::make_unique<UnsignedType>(bits);
	return std::make_unique<SignedType>(bits);
}

bool NumberExpr::isUnsigned() const {
	return literal.find('u') != std::string::npos;
}

void BoolExpr::compile(VregPtr destination, Function &function, ScopePtr, ssize_t multiplier) const {
	function.add<SetIInstruction>(destination, value? int(multiplier) : 0);
}

void VariableExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	if (VariablePtr var = scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var)) {
			function.add<LoadIInstruction>(destination, global->name);
		} else if (function.argumentMap.count(name) != 0) {
			function.addComment("Load argument " + name);
			function.add<MoveInstruction>(function.argumentMap.at(name), destination);
		} else if (function.stackOffsets.count(var) == 0) {
			throw NotOnStackError(var);
		} else {
			const size_t offset = function.stackOffsets.at(var);
			function.addComment("Load variable " + name);
			function.add<SubIInstruction>(function.precolored(Why::framePointerOffset), destination, int(offset));
			function.add<LoadRInstruction>(destination, destination);
		}
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, int(multiplier));
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
				function.add<SetIInstruction>(destination, global->name);
			else
				function.add<SubIInstruction>(function.precolored(Why::framePointerOffset), destination, var);
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
	function.add<SetIInstruction>(destination, ".str" + std::to_string(function.program.getStringID(contents)));
}

std::unique_ptr<Type> StringExpr::getType(ScopePtr) const {
	return std::make_unique<PointerType>(new UnsignedType(8));
}

void DerefExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	checkType(scope);
	subexpr->compile(destination, function, scope, multiplier);
	function.add<LoadRInstruction>(destination, destination);
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

CallExpr::CallExpr(const ASTNode &node, Function *function_): name(*node.front()->lexerInfo), function(function_) {
	if (!function)
		throw std::runtime_error("CallExpr needs a nonnull function");
	for (const ASTNode *child: *node.back())
		arguments.emplace_back(Expr::get(*child, function));
}

void CallExpr::compile(VregPtr destination, Function &fn, ScopePtr scope, ssize_t multiplier) const {
	const size_t to_push = arguments.size();
	size_t i;

	for (i = 0; i < to_push; ++i)
		fn.add<StackPushInstruction>(fn.precolored(Why::argumentOffset + i));

	i = 0;
	for (const auto &argument: arguments)
		argument->compile(fn.precolored(Why::argumentOffset + i++), fn, scope);

	fn.add<JumpInstruction>(name, true);

	Function *found = scope->lookupFunction(name);
	if (!found)
		throw std::runtime_error("Function not found: " + name);

	for (i = to_push; 0 < i; --i)
		fn.add<StackPopInstruction>(fn.precolored(Why::argumentOffset + i - 1));

	if (!found->returnType->isVoid() && destination) {
		if (multiplier == 1)
			fn.add<MoveInstruction>(fn.precolored(Why::returnValueOffset), destination);
		else
			fn.add<MultIInstruction>(fn.precolored(Why::returnValueOffset), destination, int(multiplier));
	}
}

CallExpr::operator std::string() const {
	std::stringstream out;
	out << name << '(';
	bool first = true;
	for (const auto &argument: arguments) {
		if (first)
			first = false;
		else
			out << ", ";
		out << std::string(*argument);
	}
	out << ')';
	return out.str();
}

size_t CallExpr::getSize(ScopePtr scope) const {
	if (auto fn = scope->lookupFunction(name)) {
		return fn->returnType->getSize();
	} else
		throw ResolutionError(name, scope);
}

std::unique_ptr<Type> CallExpr::getType(ScopePtr scope) const {
	if (auto fn = scope->lookupFunction(name)) {
		return std::unique_ptr<Type>(fn->returnType->copy());
	} else
		throw ResolutionError(name, scope);
}

void AssignExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) const {
	if (auto *var_expr = left->cast<VariableExpr>()) {
		if (auto var = scope->lookup(var_expr->name)) {
			if (!destination)
				destination = function.mx(1);
			right->compile(destination, function, scope, multiplier);
			if (auto *global = var->cast<Global>()) {
				function.add<StoreIInstruction>(destination, global->name);
			} else {
				auto fp = function.precolored(Why::framePointerOffset);
				auto offset = function.stackOffsets.at(var);
				if (offset == 0) {
					function.add<StoreRInstruction>(destination, fp);
				} else {
					auto m0 = function.mx(0);
					function.add<SubIInstruction>(fp, m0, int(offset));
					function.add<StoreRInstruction>(destination, m0);
				}
			}
		} else
			throw ResolutionError(var_expr->name, scope);
	} else if (auto *deref_expr = left->cast<DerefExpr>()) {
		deref_expr->checkType(scope);
		auto m0 = function.mx(0);
		if (!destination)
			destination = function.mx(1);
		right->compile(destination, function, scope, multiplier);
		deref_expr->subexpr->compile(m0, function, scope);
		function.add<StoreRInstruction>(destination, m0);
	} else
		throw LvalueError(*left->getType(scope));
}

std::unique_ptr<Type> AssignExpr::getType(ScopePtr scope) const {
	auto left_type = left->getType(scope), right_type = right->getType(scope);
	if (!(*right_type && *left_type))
			throw ImplicitConversionError(*right_type, *left_type);
	return left_type;
}

size_t AssignExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> AssignExpr::evaluate() const {
	return right? right->evaluate() : std::nullopt;
}
