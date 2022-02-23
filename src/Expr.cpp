#include <climits>
#include <iostream>
#include <sstream>

#include "ASTNode.h"
#include "Casting.h"
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

void Expr::compile(VregPtr, Function &, ScopePtr, ssize_t) {}

Expr * Expr::get(const ASTNode &node, Function *function) {
	Expr *out = nullptr;
	switch (node.symbol) {
		case CMMTOK_PLUS:
			out = new PlusExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_MINUS:
			out = new MinusExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_DIV:
			out = new DivExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_MOD:
			out = new ModExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_TIMES:
			if (node.size() == 1)
				out = new DerefExpr(Expr::get(*node.front(), function));
			else
				out = new MultExpr(
					std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
					std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_NUMBER:
			if (node.size() == 1) // Contains a "-" child indicating unary negation
				out = new NumberExpr("-" + *node.text);
			else
				out = new NumberExpr(*node.text);
			break;
		case CMMTOK_TRUE:
		case CMM_EMPTY:
			out = new BoolExpr(true);
			break;
		case CMMTOK_FALSE:
			out = new BoolExpr(false);
			break;
		case CMM_ADDROF:
			out = new AddressOfExpr(Expr::get(*node.front(), function));
			break;
		case CMMTOK_TILDE:
			out = new NotExpr(Expr::get(*node.front(), function));
			break;
		case CMMTOK_NOT:
			out = new LnotExpr(Expr::get(*node.front(), function));
			break;
		case CMMTOK_HASH:
			out = new LengthExpr(Expr::get(*node.front(), function));
			break;
		case CMMTOK_PLUSPLUS:
			out = new PrefixPlusExpr(Expr::get(*node.front(), function));
			break;
		case CMMTOK_MINUSMINUS:
			out = new PrefixMinusExpr(Expr::get(*node.front(), function));
			break;
		case CMM_POSTPLUS:
			out = new PostfixPlusExpr(Expr::get(*node.front(), function));
			break;
		case CMM_POSTMINUS:
			out = new PostfixMinusExpr(Expr::get(*node.front(), function));
			break;
		case CMMTOK_PERIOD:
			out = new DotExpr(Expr::get(*node.front(), function), *node.at(1)->text);
			break;
		case CMMTOK_ARROW:
			out = new ArrowExpr(Expr::get(*node.front(), function), *node.at(1)->text);
			break;
		case CMMTOK_IDENT:
			if (!function)
				throw std::runtime_error("Variable expression encountered in functionless context");
			out = new VariableExpr(*node.text);
			break;
		case CMMTOK_LPAREN: {
			std::vector<ExprPtr> arguments;
			arguments.reserve(node.at(1)->size());
			for (const ASTNode *child: *node.at(1))
				arguments.emplace_back(Expr::get(*child, function));
			out = new CallExpr(Expr::get(*node.front(), function), arguments);
			break;
		}
		case CMMTOK_STRING:
			out = new StringExpr(node.unquote());
			break;
		case CMMTOK_CHAR:
			out = new NumberExpr(std::to_string(ssize_t(node.getChar())) + "u8");
			break;
		case CMMTOK_LT:
			out = new LtExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_LTE:
			out = new LteExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_GT:
			out = new GtExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_GTE:
			out = new GteExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_DEQ:
			out = new EqExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_NEQ:
			out = new NeqExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_ASSIGN:
			out = new AssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMM_CAST:
			out = new CastExpr(Type::get(*node.at(0), function->program), Expr::get(*node.at(1), function));
			break;
		case CMMTOK_LSHIFT:
			out = new ShiftLeftExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_RSHIFT:
			out = new ShiftRightExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_AND:
			out = new AndExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_OR:
			out = new OrExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_LAND:
			out = new LandExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_LOR:
			out = new LorExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_LSQUARE:
			out = new AccessExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_QUESTION:
			out = new TernaryExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(2), function)));
			break;
		default:
			throw std::invalid_argument("Unrecognized symbol in Expr::get: " +
				std::string(cmmParser.getName(node.symbol)));
	}

	return out->setLocation(node.location);
}

std::ostream & operator<<(std::ostream &os, const Expr &expr) {
	return os << std::string(expr);
}

void PlusExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
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

std::optional<ssize_t> PlusExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
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

void MinusExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
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

void MultExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	left->compile(left_var, function, scope, 1);
	right->compile(right_var, function, scope, multiplier); // TODO: verify
	function.add<MultRInstruction>(left_var, right_var, destination);
}

void ShiftLeftExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr temp_var = function.newVar();
	left->compile(temp_var, function, scope, multiplier);
	right->compile(destination, function, scope);
	function.add<ShiftLeftLogicalRInstruction>(temp_var, destination, destination);
}

size_t ShiftLeftExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> ShiftLeftExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value)
		return *left_value << *right_value;
	return std::nullopt;
}

void ShiftRightExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr temp_var = function.newVar();
	left->compile(temp_var, function, scope, multiplier);
	right->compile(destination, function, scope);
	if (left->getType(scope)->isUnsigned())
		function.add<ShiftRightLogicalRInstruction>(temp_var, destination, destination);
	else
		function.add<ShiftRightArithmeticRInstruction>(temp_var, destination, destination);
}

size_t ShiftRightExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> ShiftRightExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value) {
		if (left->getType(scope)->isUnsigned())
			return size_t(*left_value) << *right_value;
		return *left_value << *right_value;
	}
	return std::nullopt;
}

void AndExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr temp_var = function.newVar();
	left->compile(temp_var, function, scope);
	right->compile(destination, function, scope);
	function.add<AndRInstruction>(temp_var, destination, destination);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

size_t AndExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> AndExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value)
		return *left_value & *right_value;
	return std::nullopt;
}

void OrExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr temp_var = function.newVar();
	left->compile(temp_var, function, scope);
	right->compile(destination, function, scope);
	function.add<OrRInstruction>(temp_var, destination, destination);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

size_t OrExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> OrExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value)
		return *left_value | *right_value;
	return std::nullopt;
}

void LandExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	const std::string base = "." + function.name + "." + std::to_string(function.getNextBlock());
	const std::string success = base + "s", end = base + "e";
	left->compile(destination, function, scope);
	function.add<JumpConditionalInstruction>(success, destination, false);
	function.add<JumpInstruction>(end);
	function.add<Label>(success);
	right->compile(destination, function, scope);
	function.add<Label>(end);

	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

size_t LandExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> LandExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value)
		return *left_value && *right_value;
	return std::nullopt;
}

void LorExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	const std::string success = "." + function.name + "." + std::to_string(function.getNextBlock()) + "s";
	left->compile(destination, function, scope);
	function.add<JumpConditionalInstruction>(success, destination, false);
	right->compile(destination, function, scope);
	function.add<Label>(success);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

size_t LorExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> LorExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value)
		return *left_value || *right_value;
	return std::nullopt;
}

void DivExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr temp_var = function.newVar();
	left->compile(temp_var, function, scope, multiplier);
	right->compile(destination, function, scope);
	if (left->getType(scope)->isUnsigned())
		function.add<DivuRInstruction>(temp_var, destination, destination);
	else
		function.add<DivRInstruction>(temp_var, destination, destination);
}

size_t DivExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> DivExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value) {
		if (left->getType(scope)->isUnsigned())
			return size_t(*left_value) / size_t(*right_value);
		return *left_value / *right_value;
	}
	return std::nullopt;
}

void ModExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr temp_var = function.newVar();
	left->compile(temp_var, function, scope, multiplier);
	right->compile(destination, function, scope);
	if (left->getType(scope)->isUnsigned())
		function.add<ModuRInstruction>(temp_var, destination, destination);
	else
		function.add<ModRInstruction>(temp_var, destination, destination);
}

size_t ModExpr::getSize(ScopePtr scope) const {
	return left->getSize(scope);
}

std::optional<ssize_t> ModExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value) {
		if (left->getType(scope)->isUnsigned())
			return size_t(*left_value) % size_t(*right_value);
		return *left_value % *right_value;
	}
	return std::nullopt;
}

ssize_t NumberExpr::getValue() const {
	getSize(nullptr);
	return Util::parseLong(literal.substr(0, literal.find_first_of("su")));
}

void NumberExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	const ssize_t multiplied = getValue() * multiplier;
	getSize(scope);
	if (!destination)
		return;
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
	const size_t suffix = literal.find_first_of("su");

	// TODO: bounds checking for unsigned literals

	if (suffix == std::string::npos)
		return 8;

	const ssize_t value = Util::parseLong(literal.substr(0, suffix));
	const size_t size = Util::parseLong(literal.substr(suffix + 1)) / 8;
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
	if (literal.find('u') != std::string::npos)
		return std::make_unique<UnsignedType>(bits);
	return std::make_unique<SignedType>(bits);
}

void BoolExpr::compile(VregPtr destination, Function &function, ScopePtr, ssize_t multiplier) {
	if (destination)
		function.add<SetIInstruction>(destination, value? size_t(multiplier) : 0);
}

void VariableExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	if (VariablePtr var = scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var)) {
			function.add<LoadIInstruction>(destination, global->name, global->getSize());
		} else if (function.argumentMap.count(name) != 0) {
			function.addComment("Load argument " + name);
			function.add<MoveInstruction>(function.argumentMap.at(name), destination);
		} else if (function.stackOffsets.count(var) == 0) {
			throw NotOnStackError(var);
		} else {
			const size_t offset = function.stackOffsets.at(var);
			function.addComment("Load variable " + name);
			function.add<SubIInstruction>(function.precolored(Why::framePointerOffset), destination, offset);
			function.add<LoadRInstruction>(destination, destination, var->getSize());
		}
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier));
	} else if (const auto *fn = scope->lookupFunction(name)) {
		function.add<SetIInstruction>(destination, name);
	} else
		throw ResolutionError(name, scope);
}

bool VariableExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	if (VariablePtr var = scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var)) {
			function.add<SetIInstruction>(destination, global->name);
		} else if (function.argumentMap.count(name) != 0 || function.stackOffsets.count(var) == 0) {
			throw NotOnStackError(var, location);
		} else {
			const size_t offset = function.stackOffsets.at(var);
			function.addComment("Get variable lvalue for " + name);
			function.add<SubIInstruction>(function.precolored(Why::framePointerOffset), destination, offset);
		}
	} else if (const auto *fn = scope->lookupFunction(name)) {
		function.add<SetIInstruction>(destination, fn->name);
	} else
		throw ResolutionError(name, scope);
	return true;
}

size_t VariableExpr::getSize(ScopePtr scope) const {
	if (VariablePtr var = scope->lookup(name))
		return var->getSize();
	else if (scope->lookupFunction(name))
		return Why::wordSize;
	throw ResolutionError(name, scope);
}

std::unique_ptr<Type> VariableExpr::getType(ScopePtr scope) const {
	if (VariablePtr var = scope->lookup(name))
		return std::unique_ptr<Type>(var->type->copy());
	else if (const auto *fn = scope->lookupFunction(name))
		return std::make_unique<FunctionPointerType>(*fn);
	throw ResolutionError(name, scope);
}

void AddressOfExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	if (multiplier != 1)
		throw std::invalid_argument("Cannot multiply in AddressOfExpr");

	if (!destination)
		return;

	if (!subexpr->compileAddress(destination, function, scope))
		throw LvalueError(*subexpr);
}

std::unique_ptr<Type> AddressOfExpr::getType(ScopePtr scope) const {
	if (!subexpr->is<VariableExpr>() && !subexpr->is<DerefExpr>() && !subexpr->is<AccessExpr>())
		throw LvalueError(*subexpr);
	return std::make_unique<PointerType>(subexpr->getType(scope)->copy());
}

void NotExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	subexpr->compile(destination, function, scope);
	function.add<NotRInstruction>(destination, destination);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

void LnotExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	subexpr->compile(destination, function, scope);
	function.add<LnotRInstruction>(destination, destination);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

void StringExpr::compile(VregPtr destination, Function &function, ScopePtr, ssize_t multiplier) {
	if (multiplier != 1)
		throw std::invalid_argument("Cannot multiply in StringExpr");
	if (destination)
		function.add<SetIInstruction>(destination, getID(function.program));
}

std::unique_ptr<Type> StringExpr::getType(ScopePtr) const {
	return std::make_unique<PointerType>(new UnsignedType(8));
}

bool StringExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	// TODO: is this correct?
	compile(destination, function, scope, 1);
	return true;
}

std::string StringExpr::getID(Program &program) const {
	return ".str" + std::to_string(program.getStringID(contents));
}

void DerefExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	checkType(scope);
	subexpr->compile(destination, function, scope, multiplier);
	function.add<LoadRInstruction>(destination, destination, getSize(scope));
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

bool DerefExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	checkType(scope);
	subexpr->compile(destination, function, scope, 1);
	return true;
}

Expr * CallExpr::copy() const {
	std::vector<ExprPtr> arguments_copy;
	for (const ExprPtr &argument: arguments)
		arguments_copy.emplace_back(argument->copy());
	return new CallExpr(subexpr->copy(), arguments_copy);
}

void CallExpr::compile(VregPtr destination, Function &fn, ScopePtr scope, ssize_t multiplier) {
	const size_t to_push = arguments.size();
	size_t i;

	std::function<const Type &(size_t)> get_arg_type = [](size_t) -> const Type & {
		throw std::logic_error("get_arg_type not redefined");
	};

	std::function<void()> add_jump = [] {
		throw std::logic_error("add_jump not redefined");
	};

	TypePtr found_return_type = nullptr;

	auto fnptr_type = subexpr->getType(scope);

	bool function_found = false;

	if (auto *var_expr = subexpr->cast<VariableExpr>()) {
		const std::string &name = var_expr->name;
		Function *found = scope->lookupFunction(name);

		if (found) {
			if (found->arguments.size() != arguments.size())
				throw std::runtime_error("Invalid number of arguments in call to " + found->name + " at " +
					std::string(location) + ": " + std::to_string(arguments.size()) + " (expected " +
					std::to_string(found->arguments.size()) + ")");

			function_found = true;
			found_return_type = found->returnType;
			get_arg_type = [found](size_t i) -> const Type & {
				return *found->argumentMap.at(found->arguments.at(i))->type;
			};
			add_jump = [&name, &fn] { fn.add<JumpInstruction>(name, true); };
		}
	}

	if (!function_found) {
		if (!fnptr_type->isFunctionPointer())
			throw FunctionPointerError(*fnptr_type);
		const auto *subfn = fnptr_type->cast<FunctionPointerType>();
		found_return_type = TypePtr(subfn->returnType->copy());
		get_arg_type = [subfn](size_t i) -> const Type & { return *subfn->argumentTypes.at(i); };
		add_jump = [this, &fn, scope] {
			auto jump_destination = fn.newVar();
			subexpr->compile(jump_destination, fn, scope);
			fn.add<JumpRegisterInstruction>(jump_destination, true);
		};
	}

	for (i = 0; i < to_push; ++i)
		fn.add<StackPushInstruction>(fn.precolored(Why::argumentOffset + i));

	i = 0;
	for (const auto &argument: arguments) {
		auto argument_register = fn.precolored(Why::argumentOffset + i);
		argument->compile(argument_register, fn, scope);
		try {
			typeCheck(*argument->getType(scope), get_arg_type(i), argument_register, fn, location);
		} catch (std::out_of_range &err) {
			std::cerr << "\e[31mBad function argument at " << argument->location << "\e[39m\n";
			throw;
		}
		++i;
	}

	add_jump();

	for (i = to_push; 0 < i; --i)
		fn.add<StackPopInstruction>(fn.precolored(Why::argumentOffset + i - 1));

	if (!found_return_type->isVoid() && destination) {
		if (multiplier == 1)
			fn.add<MoveInstruction>(fn.precolored(Why::returnValueOffset), destination);
		else
			fn.add<MultIInstruction>(fn.precolored(Why::returnValueOffset), destination, size_t(multiplier));
	}
}

CallExpr::operator std::string() const {
	std::stringstream out;
	if (auto *var_expr = subexpr->cast<VariableExpr>())
		out << var_expr->name << '(';
	else
		out << '(' << *subexpr << ")(";
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
	return getType(scope)->getSize();
}

std::unique_ptr<Type> CallExpr::getType(ScopePtr scope) const {
	if (auto *var_expr = subexpr->cast<VariableExpr>()) {
		if (const auto *fn = scope->lookupFunction(var_expr->name))
			return std::unique_ptr<Type>(fn->returnType->copy());
		if (auto var = scope->lookup(var_expr->name)) {
			if (auto *fnptr = var->type->cast<FunctionPointerType>())
				return std::unique_ptr<Type>(fnptr->returnType->copy());
			throw FunctionPointerError(*var->type);
		}
		throw ResolutionError(var_expr->name, scope);
	} else {
		auto type = subexpr->getType(scope);
		if (const auto *fnptr = type->cast<FunctionPointerType>())
				return std::unique_ptr<Type>(fnptr->returnType->copy());
		throw FunctionPointerError(*type);
	}
}

void AssignExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	if (auto *var_expr = left->cast<VariableExpr>()) {
		if (auto var = scope->lookup(var_expr->name)) {
			if (!destination)
				destination = function.newVar();
			right->compile(destination, function, scope, multiplier);
			TypePtr right_type = right->getType(scope), left_type = left->getType(scope);
			if (!tryCast(*right_type, *left_type, destination, function))
				throw ImplicitConversionError(right_type, left_type);
			if (auto *global = var->cast<Global>()) {
				function.add<StoreIInstruction>(destination, global->name, global->getSize());
			} else {
				auto fp = function.precolored(Why::framePointerOffset);
				const size_t offset = function.stackOffsets.at(var);
				if (offset == 0) {
					function.add<StoreRInstruction>(destination, fp, var->getSize());
				} else {
					auto temp = function.newVar();
					function.add<SubIInstruction>(fp, temp, offset);
					function.add<StoreRInstruction>(destination, temp, var->getSize());
				}
			}
		} else
			throw ResolutionError(var_expr->name, scope);
	} else if (auto *deref_expr = left->cast<DerefExpr>()) {
		deref_expr->checkType(scope);
		auto addr_variable = function.newVar();
		if (!destination)
			destination = function.newVar();
		right->compile(destination, function, scope, multiplier);
		deref_expr->subexpr->compile(addr_variable, function, scope);
		function.add<StoreRInstruction>(destination, addr_variable, deref_expr->getSize(scope)); // TODO: verify size
	} else if (auto *access_expr = left->cast<AccessExpr>()) {
		access_expr->check(scope);
		auto addr_variable = function.newVar();
		if (!destination)
			destination = function.newVar();
		right->compile(destination, function, scope, multiplier);
		access_expr->compileAddress(addr_variable, function, scope);
		function.add<StoreRInstruction>(destination, addr_variable, access_expr->getSize(scope)); // TODO: verify size
	} else {
		auto addr_variable = function.newVar();
		if (!destination)
			destination = function.newVar();
		right->compile(destination, function, scope, multiplier);
		if (!left->compileAddress(addr_variable, function, scope))
			throw LvalueError(*left->getType(scope));
		function.add<StoreRInstruction>(destination, addr_variable, left->getSize(scope)); // TODO: verify size
	}
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

std::optional<ssize_t> AssignExpr::evaluate(ScopePtr scope) const {
	return right? right->evaluate(scope) : std::nullopt;
}

bool AssignExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	return left? left->compileAddress(destination, function, scope) : false;
}

void CastExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	subexpr->compile(destination, function, scope, multiplier);
	tryCast(*subexpr->getType(scope), *targetType, destination, function);
}

std::unique_ptr<Type> CastExpr::getType(ScopePtr) const {
	return std::unique_ptr<Type>(targetType->copy());
}

void AccessExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	compileAddress(destination, function, scope);
	function.add<LoadRInstruction>(destination, destination, getSize(scope));
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::unique_ptr<Type> AccessExpr::getType(ScopePtr scope) const {
	auto array_type = array->getType(scope);
	if (auto *casted = array_type->cast<const ArrayType>())
		return std::unique_ptr<Type>(casted->subtype->copy());
	if (auto *casted = array_type->cast<const PointerType>())
		return std::unique_ptr<Type>(casted->subtype->copy());
	throw std::runtime_error("Can't get array access result type: array expression isn't an array or pointer type");
}

bool AccessExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	if (check(scope)->isPointer())
		array->compile(destination, function, scope);
	else if (!array->compileAddress(destination, function, scope))
		throw LvalueError(std::string(*array->getType(scope)));
	const auto element_size = getSize(scope);
	const auto subscript_value = subscript->evaluate(scope);
	if (subscript_value) {
		if (*subscript_value != 0)
			function.add<AddIInstruction>(destination, destination, size_t(*subscript_value * element_size));
	} else {
		auto subscript_variable = function.newVar();
		subscript->compile(subscript_variable, function, scope);
		if (element_size != 1)
			function.add<MultIInstruction>(subscript_variable, subscript_variable, element_size);
		function.add<AddRInstruction>(destination, subscript_variable, destination);
	}
	return true;
}

std::unique_ptr<Type> AccessExpr::check(ScopePtr scope) {
	auto type = array->getType(scope);
	const bool is_array = type->isArray(), is_pointer = type->isPointer();
	if (!is_array && !is_pointer)
		throw NotArrayError(TypePtr(type->copy()));
	if (!warned && is_array)
		if (const auto evaluated = subscript->evaluate(scope)) {
			const auto array_count = type->cast<ArrayType>()->count;
			if (*evaluated < 0 || ssize_t(array_count) <= *evaluated) {
				warn() << "Array index " << *evaluated << " at " << subscript->location << " is higher than array size ("
					<< array_count << ") at " << location << '\n';
				warned = true;
			}
		}
	return std::unique_ptr<Type>(type->copy());
}

void LengthExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	TypePtr type = subexpr->getType(scope);
	if (auto *array = type->cast<const ArrayType>())
		function.add<SetIInstruction>(destination, array->count * multiplier);
	else
		throw NotArrayError(type);
}

void TernaryExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	const std::string base = "." + function.name + "." + std::to_string(function.getNextBlock());
	const std::string true_label = base + "t", end = base + "e";
	condition->compile(destination, function, scope);
	function.add<JumpConditionalInstruction>(true_label, destination, false);
	ifFalse->compile(destination, function, scope, multiplier);
	function.add<JumpInstruction>(end);
	function.add<Label>(true_label);
	ifTrue->compile(destination, function, scope, multiplier);
	function.add<Label>(end);
}

std::unique_ptr<Type> TernaryExpr::getType(ScopePtr scope) const {
	auto condition_type = condition->getType(scope);
	if (!(*condition_type && BoolType()))
		throw ImplicitConversionError(*condition_type, BoolType());
	auto true_type = ifTrue->getType(scope), false_type = ifFalse->getType(scope);
	if (!(*true_type && *false_type) || !(*false_type && *true_type))
		throw ImplicitConversionError(*false_type, *true_type);
	return true_type;
}

size_t TernaryExpr::getSize(ScopePtr scope) const {
	return getType(scope)->getSize();
}

void DotExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	auto struct_type = checkType(scope);
	const size_t field_size = struct_type->getFieldSize(ident);
	const size_t field_offset = struct_type->getFieldOffset(ident);
	Util::validateSize(field_size);
	if (!left->compileAddress(destination, function, scope))
		throw LvalueError(*left);
	if (field_offset != 0)
		function.add<AddIInstruction>(destination, destination, field_offset);
	function.add<LoadRInstruction>(destination, destination, field_size);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::unique_ptr<Type> DotExpr::getType(ScopePtr scope) const {
	return std::unique_ptr<Type>(checkType(scope)->map.at(ident)->copy());
}

bool DotExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	auto struct_type = checkType(scope);
	const size_t field_offset = checkType(scope)->getFieldOffset(ident);
	if (!left->compileAddress(destination, function, scope))
		throw LvalueError(*left);
	if (field_offset != 0)
		function.add<AddIInstruction>(destination, destination, field_offset);
	return true;
}

std::shared_ptr<StructType> DotExpr::checkType(ScopePtr scope) const {
	auto left_type = left->getType(scope);
	if (!left_type->isStruct())
		throw NotStructError(std::move(left_type));
	TypePtr shared_type = std::move(left_type);
	return shared_type->ptrcast<StructType>();
}

size_t DotExpr::getSize(ScopePtr scope) const {
	return checkType(scope)->map.at(ident)->getSize();
}

void ArrowExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	auto struct_type = checkType(scope);
	const size_t field_size = struct_type->getFieldSize(ident);
	const size_t field_offset = struct_type->getFieldOffset(ident);
	Util::validateSize(field_size);
	left->compile(destination, function, scope, 1);
	if (field_offset != 0)
		function.add<AddIInstruction>(destination, destination, field_offset);
	function.add<LoadRInstruction>(destination, destination, field_size);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::unique_ptr<Type> ArrowExpr::getType(ScopePtr scope) const {
	return std::unique_ptr<Type>(checkType(scope)->map.at(ident)->copy());
}

bool ArrowExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	auto struct_type = checkType(scope);
	const size_t field_offset = checkType(scope)->getFieldOffset(ident);
	left->compile(destination, function, scope);
	if (field_offset != 0)
		function.add<AddIInstruction>(destination, destination, field_offset);
	return true;
}

std::shared_ptr<StructType> ArrowExpr::checkType(ScopePtr scope) const {
	auto left_type = left->getType(scope);
	if (!left_type->isPointer())
		throw NotPointerError(std::move(left_type));
	auto *left_pointer = left_type->cast<PointerType>();
	TypePtr shared_type = TypePtr(left_pointer->subtype->copy());
	if (!left_pointer->subtype->isStruct())
		throw NotStructError(shared_type);
	return shared_type->ptrcast<StructType>();
}

size_t ArrowExpr::getSize(ScopePtr scope) const {
	return checkType(scope)->map.at(ident)->getSize();
}
