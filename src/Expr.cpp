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
		case CMMTOK_NULL:
			out = new NullExpr;
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
		case CMMTOK_SIZEOF:
			if (node.size() == 2)
				out = new SizeofMemberExpr(*node.front()->text, *node.at(1)->text);
			else
				out = new SizeofExpr(TypePtr(Type::get(*node.front(), function->program)));
			break;
		case CMMTOK_OFFSETOF:
			out = new OffsetofExpr(*node.front()->text, *node.at(1)->text);
			break;
		case CMMTOK_IDENT:
			if (!function)
				throw LocatedError(node.location, "Variable expression encountered in functionless context");
			out = new VariableExpr(*node.text);
			break;
		case CMMTOK_LPAREN: {
			std::vector<ExprPtr> arguments;
			arguments.reserve(node.at(1)->size());
			for (const ASTNode *child: *node.at(1))
				arguments.emplace_back(Expr::get(*child, function));
			CallExpr *call = new CallExpr(Expr::get(*node.front(), function), arguments);
			if (node.size() == 3) {
				if (node.at(2)->symbol == CMMTOK_MOD)
					// Static struct method call
					call->structName = *node.at(2)->front()->text;
				else
					// Non-static struct method call
					call->structExpr = std::unique_ptr<Expr>(Expr::get(*node.at(2), function));
			}
			out = call;
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
		case CMMTOK_LXOR:
			out = new LxorExpr(
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
		case CMMTOK_PLUSEQ:
			out = new PlusAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_MINUSEQ:
			out = new MinusAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_DIVEQ:
			out = new DivAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_TIMESEQ:
			out = new MultAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_MODEQ:
			out = new ModAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_SREQ:
			out = new ShiftRightAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_SLEQ:
			out = new ShiftLeftAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_ANDEQ:
			out = new AndAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_OREQ:
			out = new OrAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMMTOK_XOREQ:
			out = new XorAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CMM_INITIALIZER: {
			std::vector<ExprPtr> exprs;
			for (const ASTNode *child: node)
				exprs.emplace_back(Expr::get(*child, function));
			out = new InitializerExpr(std::move(exprs));
			break;
		}
		default:
			throw LocatedError(node.location, "Unrecognized symbol in Expr::get: " +
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
			throw LocatedError(location, "Cannot multiply in pointer arithmetic PlusExpr");
		auto *left_subtype = dynamic_cast<PointerType &>(*left_type).subtype;
		left->compile(left_var, function, scope, 1);
		right->compile(right_var, function, scope, left_subtype->getSize());
	} else if (left_type->isInt() && right_type->isPointer()) {
		if (multiplier != 1)
			throw LocatedError(location, "Cannot multiply in pointer arithmetic PlusExpr");
		auto *right_subtype = dynamic_cast<PointerType &>(*right_type).subtype;
		left->compile(left_var, function, scope, right_subtype->getSize());
		right->compile(right_var, function, scope, 1);
	} else if (!(*left_type && *right_type)) {
		throw ImplicitConversionError(TypePtr(left_type->copy()), TypePtr(right_type->copy()), location);
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

std::unique_ptr<Type> PlusExpr::getType(const Context &context) const {
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (left_type->isPointer() && right_type->isInt())
		return left_type;
	if (left_type->isInt() && right_type->isPointer())
		return right_type;
	if (!(*left_type && *right_type) || !(*right_type && *left_type))
		throw ImplicitConversionError(*left_type, *right_type, location);
	return left_type;
}

void MinusExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	auto left_type = left->getType(scope), right_type = right->getType(scope);

	if (left_type->isPointer() && right_type->isInt()) {
		if (multiplier != 1)
			throw LocatedError(location, "Cannot multiply in pointer arithmetic MinusExpr");
		auto *left_subtype = dynamic_cast<PointerType &>(*left_type).subtype;
		left->compile(left_var, function, scope, 1);
		right->compile(right_var, function, scope, left_subtype->getSize());
	} else if (left_type->isInt() && right_type->isPointer()) {
		throw LocatedError(location, "Cannot subtract " + std::string(*right_type) + " from " +
			std::string(*left_type));
	} else if (!(*left_type && *right_type) && !(left_type->isPointer() && right_type->isPointer())) {
		throw ImplicitConversionError(TypePtr(left_type->copy()), TypePtr(right_type->copy()), location);
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

std::unique_ptr<Type> MinusExpr::getType(const Context &context) const {
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (left_type->isPointer() && right_type->isInt())
		return left_type;
	if (left_type->isPointer() && right_type->isPointer())
		return std::make_unique<SignedType>(64);
	if (!(*left_type && *right_type) || !(*right_type && *left_type))
		throw ImplicitConversionError(*left_type, *right_type, location);
	return left_type;
}

void ShiftLeftExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	VregPtr temp_var = function.newVar();
	left->compile(temp_var, function, scope, multiplier);
	right->compile(destination, function, scope);
	function.add<ShiftLeftLogicalRInstruction>(temp_var, destination, destination);
}

size_t ShiftLeftExpr::getSize(const Context &context) const {
	return left->getSize(context);
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

size_t ShiftRightExpr::getSize(const Context &context) const {
	return left->getSize(context);
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

size_t AndExpr::getSize(const Context &context) const {
	return left->getSize(context);
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

size_t OrExpr::getSize(const Context &context) const {
	return left->getSize(context);
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
	const std::string success = base + "land.s", end = base + "land.e";
	left->compile(destination, function, scope);
	function.add<JumpConditionalInstruction>(success, destination, false);
	function.add<JumpInstruction>(end);
	function.add<Label>(success);
	right->compile(destination, function, scope);
	function.add<Label>(end);

	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::optional<ssize_t> LandExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value)
		return *left_value && *right_value;
	return std::nullopt;
}

void LorExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	const std::string success = "." + function.name + "." + std::to_string(function.getNextBlock()) + "lor.s";
	left->compile(destination, function, scope);
	function.add<JumpConditionalInstruction>(success, destination, false);
	right->compile(destination, function, scope);
	function.add<Label>(success);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::optional<ssize_t> LorExpr::evaluate(ScopePtr scope) const {
	auto left_value  = left?  left->evaluate(scope)  : std::nullopt,
	     right_value = right? right->evaluate(scope) : std::nullopt;
	if (left_value && right_value)
		return *left_value || *right_value;
	return std::nullopt;
}

void LxorExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	auto temp_var = function.newVar();
	left->compile(destination, function, scope);
	right->compile(temp_var, function, scope);
	function.add<LxorRInstruction>(destination, temp_var, destination);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::optional<ssize_t> LxorExpr::evaluate(ScopePtr scope) const {
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

size_t DivExpr::getSize(const Context &context) const {
	return left->getSize(context);
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

size_t ModExpr::getSize(const Context &context) const {
	return left->getSize(context);
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
	getSize({});
	return Util::parseLong(literal.substr(0, literal.find_first_of("su")));
}

void NumberExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	const ssize_t multiplied = getValue() * multiplier;
	getSize({scope});
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

size_t NumberExpr::getSize(const Context &) const {
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
			throw LocatedError(location, "Invalid numeric literal size (in bytes): " + std::to_string(size));
	}

	if (out_of_range)
		throw LocatedError(location, "Numeric literal cannot fit in " + std::to_string(size) + " byte" +
			(size == 1? "" : "s") + ": " + std::to_string(value));

	return size;
}

std::unique_ptr<Type> NumberExpr::getType(const Context &context) const {
	const size_t bits = getSize(context) * 8;
	if (literal.find('u') != std::string::npos)
		return std::make_unique<UnsignedType>(bits);
	return std::make_unique<SignedType>(bits);
}

void BoolExpr::compile(VregPtr destination, Function &function, ScopePtr, ssize_t multiplier) {
	if (destination)
		function.add<SetIInstruction>(destination, value? size_t(multiplier) : 0);
}

void NullExpr::compile(VregPtr destination, Function &function, ScopePtr, ssize_t) {
	if (destination)
		function.add<SetIInstruction>(destination, 0);
}

void VariableExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	if (VariablePtr var = scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var)) {
			function.add<LoadIInstruction>(destination, global->name, global->getSize());
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
		return;
	} else if (const auto fn = scope->lookupFunction(name, location)) {
		function.add<SetIInstruction>(destination, fn->mangle());
	} else
		throw ResolutionError(name, scope, location);
}

bool VariableExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	if (VariablePtr var = scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var)) {
			function.add<SetIInstruction>(destination, global->name);
		} else if (function.stackOffsets.count(var) == 0) {
			throw NotOnStackError(var, location);
		} else {
			const size_t offset = function.stackOffsets.at(var);
			function.addComment("Get variable lvalue for " + name);
			function.add<SubIInstruction>(function.precolored(Why::framePointerOffset), destination, offset);
		}
	} else if (const auto fn = scope->lookupFunction(name, location)) {
		function.add<SetIInstruction>(destination, fn->mangle());
	} else
		throw ResolutionError(name, scope, location);
	return true;
}

size_t VariableExpr::getSize(const Context &context) const {
	if (VariablePtr var = context.scope->lookup(name))
		return var->getSize();
	else if (context.scope->lookupFunction(name, nullptr, {}, location))
		return Why::wordSize;
	throw ResolutionError(name, context.scope, location);
}

std::unique_ptr<Type> VariableExpr::getType(const Context &context) const {
	if (VariablePtr var = context.scope->lookup(name))
		return std::unique_ptr<Type>(var->type->copy());
	else if (const auto fn = context.scope->lookupFunction(name, nullptr, {}, context.structName, location))
		return std::make_unique<FunctionPointerType>(*fn);
	throw ResolutionError(name, context.scope, location);
}

void AddressOfExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	if (multiplier != 1)
		throw LocatedError(location, "Cannot multiply in AddressOfExpr");

	if (!destination)
		return;

	if (!subexpr->compileAddress(destination, function, scope))
		throw LvalueError(*subexpr);
}

std::unique_ptr<Type> AddressOfExpr::getType(const Context &context) const {
	if (!subexpr->is<VariableExpr>() && !subexpr->is<DerefExpr>() && !subexpr->is<AccessExpr>())
		if (!subexpr->is<ArrowExpr>() && !subexpr->is<DotExpr>())
			throw LvalueError(*subexpr);
	return std::make_unique<PointerType>(subexpr->getType(context.scope)->copy());
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
		throw LocatedError(location, "Cannot multiply in StringExpr");
	if (destination)
		function.add<SetIInstruction>(destination, getID(function.program));
}

std::unique_ptr<Type> StringExpr::getType(const Context &) const {
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
	function.add<LoadRInstruction>(destination, destination, getSize({scope}));
}

size_t DerefExpr::getSize(const Context &context) const {
	auto type = checkType(context.scope);
	return dynamic_cast<PointerType &>(*type).subtype->getSize();
}

std::unique_ptr<Type> DerefExpr::getType(const Context &context) const {
	auto type = checkType(context.scope);
	return std::unique_ptr<Type>(dynamic_cast<PointerType &>(*type).subtype->copy());
}

std::unique_ptr<Type> DerefExpr::checkType(ScopePtr scope) const {
	auto type = subexpr->getType(scope);
	if (!type->isPointer())
		throw NotPointerError(TypePtr(type->copy()), location);
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
	std::function<const Type &(size_t)> get_arg_type = [this](size_t) -> const Type & {
		throw LocatedError(location, "get_arg_type not redefined");
	};

	std::function<void()> add_jump = [this] {
		throw LocatedError(location, "add_jump not redefined");
	};

	TypePtr found_return_type = nullptr;

	Context context(scope);
	context.structName = getStructName(scope);

	bool function_found = false;
	int argument_offset = Why::argumentOffset;

	std::unique_ptr<Type> struct_expr_type;

	size_t registers_used = (structExpr? 1 : 0) + arguments.size();
	if (Why::argumentCount < registers_used)
		throw std::runtime_error("Functions with more than 16 arguments aren't currently supported.");

	if (0 < registers_used)
		fn.addComment("Pushing " + std::to_string(registers_used) + " register" + (registers_used == 1? "" : "s"));
	for (size_t i = 0; i < registers_used; ++i)
		fn.add<StackPushInstruction>(fn.precolored(Why::argumentOffset + i));

	if (structExpr) {
		struct_expr_type = structExpr->getType(scope);
		auto this_var = fn.precolored(argument_offset++);
		if (!struct_expr_type->isStruct()) {
			if (const auto *pointer_type = struct_expr_type->cast<PointerType>())
				if (pointer_type->subtype->isStruct()) {
					fn.addComment("Setting \"this\" from pointer.");
					structExpr->compile(this_var, fn, scope);
					goto this_done; // Hehe :)
				}
			throw NotStructError(TypePtr(struct_expr_type->copy()), structExpr->location);
		} else {
			fn.addComment("Setting \"this\" from struct.");
			if (!structExpr->compileAddress(this_var, fn, scope))
				throw LvalueError(*struct_expr_type, structExpr->location);
		}
		this_done:
		fn.addComment("Done setting \"this\".");
	}

	FunctionPtr found;

	if (auto *var_expr = subexpr->cast<VariableExpr>())
		found = findFunction(var_expr->name, context);

	if (found) {
		if (found->argumentCount() != arguments.size())
			throw LocatedError(location, "Invalid number of arguments in call to " + found->name + " at " +
				std::string(location) + ": " + std::to_string(arguments.size()) + " (expected " +
				std::to_string(found->argumentCount()) + ")");

		function_found = true;
		found_return_type = found->returnType;
		get_arg_type = [found](size_t i) -> const Type & {
			return *found->getArgumentType(i);
		};
		add_jump = [found, &fn] { fn.add<JumpInstruction>(found->mangle(), true); };
	}

	std::unique_ptr<Type> fnptr_type;

	if (!function_found) {
		fnptr_type = subexpr->getType(context);
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

	size_t i = 0;

	for (const auto &argument: arguments) {
		auto argument_register = fn.precolored(argument_offset + i);
		auto argument_type = argument->getType(scope);
		if (argument_type->isStruct())
			throw LocatedError(argument->location, "Structs cannot be directly passed to functions; use a pointer");
		argument->compile(argument_register, fn, scope);
		try {
			typeCheck(*argument_type, get_arg_type(i), argument_register, fn, location);
		} catch (std::out_of_range &err) {
			std::cerr << "\e[31mBad function argument at " << argument->location << "\e[39m\n";
			throw;
		}
		++i;
	}

	add_jump();

	if (0 < registers_used)
		fn.addComment("Popping " + std::to_string(registers_used) + " register" + (registers_used == 1? "" : "s"));
	for (size_t i = registers_used; 0 < i; --i)
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

size_t CallExpr::getSize(const Context &context) const {
	return getType(context)->getSize();
}

std::unique_ptr<Type> CallExpr::getType(const Context &context) const {
	if (auto *var_expr = subexpr->cast<VariableExpr>()) {
		if (const auto fn = findFunction(var_expr->name, context))
			return std::unique_ptr<Type>(fn->returnType->copy());
		if (auto var = context.scope->lookup(var_expr->name)) {
			if (auto *fnptr = var->type->cast<FunctionPointerType>())
				return std::unique_ptr<Type>(fnptr->returnType->copy());
			throw FunctionPointerError(*var->type);
		}
		throw ResolutionError(var_expr->name, context.scope, location);
	} else {
		auto type = subexpr->getType(context);
		if (const auto *fnptr = type->cast<FunctionPointerType>())
				return std::unique_ptr<Type>(fnptr->returnType->copy());
		throw FunctionPointerError(*type);
	}
}

FunctionPtr CallExpr::findFunction(const std::string &name, const Context &context) const {
	Types arg_types;
	arg_types.reserve(arguments.size());
	for (const auto &expr: arguments)
		arg_types.push_back(expr->getType(context));
	return context.scope->lookupFunction(name, arg_types, getStructName(context), location);
}

std::string CallExpr::getStructName(const Context &context) const {
	if (structExpr) {
		auto type = structExpr->getType(context);

		if (const auto *struct_type = type->cast<StructType>())
			return struct_type->name;

		if (const auto *pointer_type = type->cast<PointerType>())
			if (const auto *struct_type = pointer_type->subtype->cast<StructType>())
				return struct_type->name;

		throw NotStructError(std::move(type), structExpr->location);
	}

	return structName;
}

void AssignExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	auto addr_var = function.newVar();
	TypePtr left_type = left->getType(scope);
	if (left_type->isConst)
		throw ConstError("Can't assign", *left_type, location);
	if (!destination)
		destination = function.newVar();
	TypePtr right_type = right->getType(scope);
	if (!left->compileAddress(addr_var, function, scope))
		throw LvalueError(*left->getType(scope));
	if (right_type->isInitializer()) {
		if (!tryCast(*right_type, *left_type, nullptr, function))
			throw ImplicitConversionError(right_type, left_type, location);
		right->cast<InitializerExpr>()->fullCompile(addr_var, function, scope);
	} else {
		right->compile(destination, function, scope);
		if (!tryCast(*right_type, *left_type, destination, function))
			throw ImplicitConversionError(right_type, left_type, location);
		function.add<StoreRInstruction>(destination, addr_var, getSize({scope}));
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier));
	}
}

std::unique_ptr<Type> AssignExpr::getType(const Context &context) const {
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (!(*right_type && *left_type))
			throw ImplicitConversionError(*right_type, *left_type, location);
	return left_type;
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

std::unique_ptr<Type> CastExpr::getType(const Context &) const {
	return std::unique_ptr<Type>(targetType->copy());
}

void AccessExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	compileAddress(destination, function, scope);
	function.add<LoadRInstruction>(destination, destination, getSize({scope}));
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::unique_ptr<Type> AccessExpr::getType(const Context &context) const {
	auto array_type = array->getType(context);
	if (auto *casted = array_type->cast<const ArrayType>())
		return std::unique_ptr<Type>(casted->subtype->copy());
	if (auto *casted = array_type->cast<const PointerType>())
		return std::unique_ptr<Type>(casted->subtype->copy());
	throw LocatedError(location, "Can't get array access result type: array expression isn't an array or pointer type");
}

bool AccessExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	if (check(scope)->isPointer())
		array->compile(destination, function, scope);
	else if (!array->compileAddress(destination, function, scope))
		throw LvalueError(std::string(*array->getType(scope)));
	const auto element_size = getSize({scope});
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
	const std::string true_label = base + "t.t", end = base + "t.e";
	condition->compile(destination, function, scope);
	function.add<JumpConditionalInstruction>(true_label, destination, false);
	ifFalse->compile(destination, function, scope, multiplier);
	function.add<JumpInstruction>(end);
	function.add<Label>(true_label);
	ifTrue->compile(destination, function, scope, multiplier);
	function.add<Label>(end);
}

std::unique_ptr<Type> TernaryExpr::getType(const Context &context) const {
	auto condition_type = condition->getType(context);
	if (!(*condition_type && BoolType()))
		throw ImplicitConversionError(*condition_type, BoolType(), location);
	auto true_type = ifTrue->getType(context), false_type = ifFalse->getType(context);
	if (!(*true_type && *false_type) || !(*false_type && *true_type))
		throw ImplicitConversionError(*false_type, *true_type, location);
	return true_type;
}

size_t TernaryExpr::getSize(const Context &context) const {
	return getType(context)->getSize();
}

void DotExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	auto struct_type = checkType({scope});
	const size_t field_size = struct_type->getFieldSize(ident);
	const size_t field_offset = struct_type->getFieldOffset(ident);
	Util::validateSize(field_size);
	if (!left->compileAddress(destination, function, scope))
		throw LvalueError(*left);
	if (field_offset != 0) {
		function.addComment("Add field offset of " + struct_type->name + "::" + ident);
		function.add<AddIInstruction>(destination, destination, field_offset);
	} else
		function.addComment("Field offset of " + struct_type->name + "::" + ident + " is 0");
	function.addComment("Load field " + struct_type->name + "::" + ident);
	function.add<LoadRInstruction>(destination, destination, field_size);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::unique_ptr<Type> DotExpr::getType(const Context &context) const {
	auto struct_type = checkType(context.scope);
	auto out = std::unique_ptr<Type>(struct_type->getMap().at(ident)->copy());
	if (struct_type->isConst)
		out->setConst(true);
	return out;
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
		throw NotStructError(std::move(left_type), location);
	TypePtr shared_type = std::move(left_type);
	return shared_type->ptrcast<StructType>();
}

size_t DotExpr::getSize(const Context &context) const {
	return checkType(context.scope)->getMap().at(ident)->getSize();
}

void ArrowExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	auto struct_type = checkType(scope);
	const size_t field_size = struct_type->getFieldSize(ident);
	const size_t field_offset = struct_type->getFieldOffset(ident);
	Util::validateSize(field_size);
	left->compile(destination, function, scope, 1);
	if (field_offset != 0) {
		function.addComment("Add field offset of " + struct_type->name + "::" + ident);
		function.add<AddIInstruction>(destination, destination, field_offset);
	} else
		function.addComment("Field offset of " + struct_type->name + "::" + ident + " is 0");
	function.addComment("Load field " + struct_type->name + "::" + ident);
	function.add<LoadRInstruction>(destination, destination, field_size);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

std::unique_ptr<Type> ArrowExpr::getType(const Context &context) const {
	auto struct_type = checkType(context.scope);
	auto out = std::unique_ptr<Type>(struct_type->getMap().at(ident)->copy());
	if (struct_type->isConst)
		out->setConst(true);
	return out;
}

bool ArrowExpr::compileAddress(VregPtr destination, Function &function, ScopePtr scope) {
	auto struct_type = checkType(scope);
	const size_t field_offset = checkType(scope)->getFieldOffset(ident);
	left->compile(destination, function, scope);
	if (field_offset != 0) {
		function.addComment("Add field offset of " + struct_type->name + "::" + ident);
		function.add<AddIInstruction>(destination, destination, field_offset);
	} else
		function.addComment("Field offset of " + struct_type->name + "::" + ident + " is 0");
	return true;
}

std::shared_ptr<StructType> ArrowExpr::checkType(ScopePtr scope) const {
	auto left_type = left->getType(scope);
	if (!left_type->isPointer())
		throw NotPointerError(std::move(left_type), location);
	auto *left_pointer = left_type->cast<PointerType>();
	TypePtr shared_type = TypePtr(left_pointer->subtype->copy());
	if (!left_pointer->subtype->isStruct())
		throw NotStructError(shared_type, location);
	return shared_type->ptrcast<StructType>();
}

size_t ArrowExpr::getSize(const Context &context) const {
	return checkType(context.scope)->getMap().at(ident)->getSize();
}

void SizeofExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	function.add<SetIInstruction>(destination, size_t(*evaluate(scope) * multiplier));
}

std::optional<ssize_t> OffsetofExpr::evaluate(ScopePtr scope) const {
	auto type = scope->lookupType(structName);
	StructType *struct_type;
	if (!type || !(struct_type = type->cast<StructType>()))
		throw LocatedError(location, "Unknown or incomplete struct in offsetof expression: " + structName);
	ssize_t offset = 0;
	bool found = false;
	for (const auto &[field_name, field_type]: struct_type->getOrder()) {
		if (field_name == fieldName) {
			found = true;
			break;
		}
		offset += field_type->getSize();
	}
	if (!found)
		throw LocatedError(location, "Struct " + structName + " has no field " + fieldName);
	return offset;
}

void OffsetofExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	function.add<SetIInstruction>(destination, size_t(*evaluate(scope) * multiplier));
}

std::optional<ssize_t> SizeofMemberExpr::evaluate(ScopePtr scope) const {
	auto type = scope->lookupType(structName);
	StructType *struct_type;
	if (!type || !(struct_type = type->cast<StructType>()))
		throw LocatedError(location, "Unknown or incomplete struct in sizeof expression: " + structName);
	const auto &map = struct_type->getMap();
	if (map.count(fieldName) == 0)
		throw LocatedError(location, "Struct " + structName + " has no field " + fieldName);
	return map.at(fieldName)->getSize();
}

void SizeofMemberExpr::compile(VregPtr destination, Function &function, ScopePtr scope, ssize_t multiplier) {
	function.add<SetIInstruction>(destination, size_t(*evaluate(scope) * multiplier));
}

Expr * InitializerExpr::copy() const {
	std::vector<ExprPtr> children_copy;
	for (const auto &child: children)
		children_copy.emplace_back(child->copy());
	return new InitializerExpr(children_copy);
}

std::unique_ptr<Type> InitializerExpr::getType(const Context &context) const {
	return std::make_unique<InitializerType>(children, context.scope);
}

void InitializerExpr::compile(VregPtr, Function &, ScopePtr, ssize_t) {
	throw LocatedError(location, "Can't compile initializers directly");
}

void InitializerExpr::fullCompile(VregPtr address, Function &function, ScopePtr scope) {
	if (children.empty())
		return;
	auto temp_var = function.newVar();
	for (const auto &child: children) {
		auto child_type = child->getType(scope);
		if (child_type->isInitializer()) {
			child->cast<InitializerExpr>()->fullCompile(address, function, scope);
		} else {
			child->compile(temp_var, function, scope);
			const size_t size = child->getSize({scope});
			function.add<StoreRInstruction>(temp_var, address, size);
			function.add<AddIInstruction>(address, address, size);
		}
	}
}
