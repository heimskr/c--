#include <climits>
#include <iostream>
#include <sstream>
#include <variant>

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

void compileCall(VregPtr destination, Function &function, const Context &context, FunctionPtr fnptr,
                 const std::vector<Argument> &arguments, const ASTLocation &location, size_t multiplier) {
	if (arguments.size() != fnptr->argumentCount())
		throw GenericError(location, "Invalid number of arguments in call to " + fnptr->name + " at " +
			std::string(location) + ": " + std::to_string(arguments.size()) + " (expected " +
			std::to_string(fnptr->argumentCount()) + ")");

	const DebugData debug(location, function);

	function.add<CallPushPlaceholder>();

	for (size_t i = 0; i < arguments.size(); ++i)
		function.add<StackPushInstruction>(function.precolored(Why::argumentOffset + i))->setDebug(debug);

	size_t i = 0;

	for (const auto &argument: arguments) {
		auto argument_register = function.precolored(Why::argumentOffset + i);
		auto &fn_arg_type = *fnptr->getArgumentType(i);
		if (std::holds_alternative<Expr *>(argument)) {
			auto expr = std::get<Expr *>(argument);
			auto argument_type = expr->getType(context);
			if (fn_arg_type.isReference()) {
				function.addComment("compileCall: compiling address into reference argument");
				if (!expr->compileAddress(argument_register, function, context))
					throw LvalueError(*argument_type, expr->getLocation());
			} else if (argument_type->isStruct()) {
				throw GenericError(expr->getLocation(),
					"Structs cannot be directly passed to functions; use a pointer");
			} else
				expr->compile(argument_register, function, context);
			try {
				typeCheck(*argument_type, fn_arg_type, argument_register, function, expr->getLocation());
			} catch (std::out_of_range &err) {
				error() << "\e[31mBad function argument at " << expr->getLocation() << "\e[39m\n";
				throw;
			}
		} else {
			auto vreg = std::get<VregPtr>(argument);
			function.add<MoveInstruction>(vreg, argument_register)->setDebug(debug);
			if (auto vreg_type = vreg->getType())
				try {
					typeCheck(*vreg_type, fn_arg_type, argument_register, function, location);
				} catch (std::out_of_range &err) {
					error() << "\e[31mBad function argument at position " + std::to_string(i + 1) << " at "
					        << location << "\e[39m\n";
					throw;
				}
		}
		++i;
	}

	function.add<JumpInstruction>(fnptr->mangle(), true)->setDebug(debug);

	for (size_t i = arguments.size(); 0 < i; --i)
		function.add<StackPopInstruction>(function.precolored(Why::argumentOffset + i - 1))->setDebug(debug);

	function.add<CallPopPlaceholder>();

	if (!fnptr->returnType->isVoid() && destination) {
		auto r0 = function.precolored(Why::returnValueOffset);
		if (multiplier == 1)
			function.add<MoveInstruction>(r0, destination)->setDebug(debug);
		else
			function.add<MultIInstruction>(r0, destination, multiplier)->setDebug(debug);
	}
}

std::string stringify(const Expr *expr) {
	if (!expr)
		return "...";
	if (expr->shouldParenthesize())
		return "(" + std::string(*expr) + ")";
	return std::string(*expr);
}

void Expr::compile(VregPtr, Function &, const Context &, ssize_t) {}

Expr * Expr::setFunction(const Function &function) {
	debug.mangledFunction = function.mangle();
	return this;
}

Expr * Expr::setDebug(const DebugData &debug_) {
	debug = debug_;
	return this;
}

Expr * Expr::get(const ASTNode &node, Function *function) {
	Expr *out = nullptr;
	switch (node.symbol) {
		case CPMTOK_PLUS:
			out = new PlusExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_MINUS:
			out = new MinusExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_DIV:
			out = new DivExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_MOD:
			out = new ModExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_TIMES:
			if (node.size() == 1)
				out = new DerefExpr(Expr::get(*node.front(), function));
			else
				out = new MultExpr(
					std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
					std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_NUMBER:
			if (node.size() == 1) // Contains a "-" child indicating unary negation
				out = new NumberExpr("-" + *node.text);
			else
				out = new NumberExpr(*node.text);
			break;
		case CPMTOK_TRUE:
		case CPM_EMPTY:
			out = new BoolExpr(true);
			break;
		case CPMTOK_FALSE:
			out = new BoolExpr(false);
			break;
		case CPMTOK_NULL:
			out = new NullExpr;
			break;
		case CPM_ADDROF:
			out = new AddressOfExpr(Expr::get(*node.front(), function));
			break;
		case CPMTOK_TILDE:
			out = new NotExpr(Expr::get(*node.front(), function));
			break;
		case CPMTOK_NOT:
			out = new LnotExpr(Expr::get(*node.front(), function));
			break;
		case CPMTOK_HASH:
			out = new LengthExpr(Expr::get(*node.front(), function));
			break;
		case CPMTOK_PLUSPLUS:
			out = new PrefixPlusExpr(Expr::get(*node.front(), function));
			break;
		case CPMTOK_MINUSMINUS:
			out = new PrefixMinusExpr(Expr::get(*node.front(), function));
			break;
		case CPM_POSTPLUS:
			out = new PostfixPlusExpr(Expr::get(*node.front(), function));
			break;
		case CPM_POSTMINUS:
			out = new PostfixMinusExpr(Expr::get(*node.front(), function));
			break;
		case CPMTOK_PERIOD:
			out = new DotExpr(Expr::get(*node.front(), function), *node.at(1)->text);
			break;
		case CPMTOK_ARROW:
			out = new ArrowExpr(Expr::get(*node.front(), function), *node.at(1)->text);
			break;
		case CPMTOK_SIZEOF:
			if (node.size() == 2)
				out = new SizeofMemberExpr(*node.front()->text, *node.at(1)->text);
			else
				out = new SizeofExpr(TypePtr(Type::get(*node.front(), function->program)));
			break;
		case CPMTOK_OFFSETOF:
			out = new OffsetofExpr(*node.front()->text, *node.at(1)->text);
			break;
		case CPMTOK_IDENT:
			if (!function)
				throw GenericError(node.location, "Variable expression encountered in functionless context");
			out = new VariableExpr(*node.text);
			break;
		case CPMTOK_LPAREN: {
			std::vector<ExprPtr> arguments;
			arguments.reserve(node.at(1)->size());
			for (const ASTNode *child: *node.at(1))
				arguments.emplace_back(Expr::get(*child, function));

			if (node.front()->symbol == CPMTOK_MOD) {
				if (!function)
					throw GenericError(node.location, "Cannot find struct in functionless context");

				const std::string &struct_name = *node.front()->front()->text;

				if (function->program.structs.count(struct_name) == 0)
					throw ResolutionError(struct_name, function->selfScope, node.location);

				auto struct_type = function->program.structs.at(struct_name);
				const size_t stack_offset = function->stackUsage += struct_type->getSize();

				auto *constructor = new ConstructorExpr(stack_offset, struct_name, std::move(arguments));
				out = constructor->addToScope(Context(function->program, function->currentScope()));
			} else {
				auto front_expr = Expr::get(*node.front(), function);
				CallExpr *call = new CallExpr(front_expr, arguments);
				call->setDebug(front_expr->debug);

				if (node.size() == 3) {
					if (node.at(2)->symbol == CPMTOK_MOD)
						// Static struct method call
						call->structName = *node.at(2)->front()->text;
					else
						// Non-static struct method call
						call->structExpr = std::unique_ptr<Expr>(Expr::get(*node.at(2), function));
				}

				out = call;
			}
			break;
		}
		case CPMTOK_NEW: {
			std::vector<ExprPtr> arguments;
			arguments.reserve(node.at(1)->size());
			for (const ASTNode *child: *node.at(1))
				arguments.emplace_back(Expr::get(*child, function));
			out = new NewExpr(TypePtr(Type::get(*node.front(), function->program)), std::move(arguments));
			break;
		}
		case CPMTOK_STRING:
			out = new StringExpr(node.unquote());
			break;
		case CPMTOK_CHAR:
			out = new NumberExpr(std::to_string(ssize_t(node.getChar())) + "u8");
			break;
		case CPMTOK_LT:
			out = new LtExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_LTE:
			out = new LteExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_GT:
			out = new GtExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_GTE:
			out = new GteExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_DEQ:
			out = new EqExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_NEQ:
			out = new NeqExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_ASSIGN:
			out = new AssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPM_CAST:
			out = new CastExpr(Type::get(*node.at(0), function->program), Expr::get(*node.at(1), function));
			break;
		case CPMTOK_LSHIFT:
			out = new ShiftLeftExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_RSHIFT:
			out = new ShiftRightExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_AND:
			out = new AndExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_OR:
			out = new OrExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_XOR:
			out = new XorExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_LAND:
			out = new LandExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_LXOR:
			out = new LxorExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_LOR:
			out = new LorExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_LSQUARE:
			out = new AccessExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_QUESTION:
			out = new TernaryExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(2), function)));
			break;
		case CPMTOK_PLUSEQ:
			out = new PlusAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_MINUSEQ:
			out = new MinusAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_DIVEQ:
			out = new DivAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_TIMESEQ:
			out = new MultAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_MODEQ:
			out = new ModAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_SREQ:
			out = new ShiftRightAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_SLEQ:
			out = new ShiftLeftAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_ANDEQ:
			out = new AndAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_OREQ:
			out = new OrAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPMTOK_XOREQ:
			out = new XorAssignExpr(
				std::unique_ptr<Expr>(Expr::get(*node.at(0), function)),
				std::unique_ptr<Expr>(Expr::get(*node.at(1), function)));
			break;
		case CPM_INITIALIZER: {
			std::vector<ExprPtr> exprs;
			for (const ASTNode *child: node)
				exprs.emplace_back(Expr::get(*child, function));
			out = new InitializerExpr(std::move(exprs), node.attributes.count("constructor") != 0);
			break;
		}
		case CPMTOK_SCOPE:
			out = new StaticFieldExpr(*node.front()->front()->text, *node.at(1)->text);
			break;
		default:
			throw GenericError(node.location, "Unrecognized symbol in Expr::get: " +
				std::string(cpmParser.getName(node.symbol)));
	}

	if (function)
		out->setFunction(*function);
	return out->setLocation(node.location);
}

std::ostream & operator<<(std::ostream &os, const Expr &expr) {
	return os << std::string(expr);
}

void PlusExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		auto left_type = left->getType(context), right_type = right->getType(context);
		VregPtr left_var = function.newVar(), right_var = function.newVar();
		if (left_type->isPointer() && right_type->isInt()) {
			if (multiplier != 1)
				throw GenericError(getLocation(), "Cannot multiply in pointer arithmetic PlusExpr");
			auto *left_subtype = dynamic_cast<PointerType &>(*left_type).subtype;
			left->compile(left_var, function, context, 1);
			right->compile(right_var, function, context, left_subtype->getSize());
		} else if (left_type->isInt() && right_type->isPointer()) {
			if (multiplier != 1)
				throw GenericError(getLocation(), "Cannot multiply in pointer arithmetic PlusExpr");
			auto *right_subtype = dynamic_cast<PointerType &>(*right_type).subtype;
			left->compile(left_var, function, context, right_subtype->getSize());
			right->compile(right_var, function, context, 1);
		} else if (!(*left_type && *right_type)) {
			throw ImplicitConversionError(TypePtr(left_type->copy()), TypePtr(right_type->copy()), getLocation());
		} else {
			left->compile(left_var, function, context, multiplier);
			right->compile(right_var, function, context, multiplier);
		}

		function.add<AddRInstruction>(left_var, right_var, destination)->setDebug(*this);
	}
}

std::optional<ssize_t> PlusExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value)
		return *left_value + *right_value;
	return std::nullopt;
}

std::unique_ptr<Type> PlusExpr::getType(const Context &context) const {
	if (auto fnptr = getOperator(context))
		return std::unique_ptr<Type>(fnptr->returnType->copy());
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (left_type->isPointer() && right_type->isInt())
		return left_type;
	if (left_type->isInt() && right_type->isPointer())
		return right_type;
	if (!(*left_type && *right_type) || !(*right_type && *left_type))
		throw ImplicitConversionError(*left_type, *right_type, getLocation());
	return left_type;
}

void MinusExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (auto fnptr = function.program.getOperator({left_type.get(), right_type.get()}, CPMTOK_MINUS, getLocation())) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr left_var = function.newVar(), right_var = function.newVar();
		if (left_type->isPointer() && right_type->isInt()) {
			if (multiplier != 1)
				throw GenericError(getLocation(), "Cannot multiply in pointer arithmetic MinusExpr");
			auto *left_subtype = dynamic_cast<PointerType &>(*left_type).subtype;
			left->compile(left_var, function, context, 1);
			right->compile(right_var, function, context, left_subtype->getSize());
		} else if (left_type->isInt() && right_type->isPointer()) {
			throw GenericError(getLocation(), "Cannot subtract " + std::string(*right_type) + " from " +
				std::string(*left_type));
		} else if (!(*left_type && *right_type) && !(left_type->isPointer() && right_type->isPointer())) {
			throw ImplicitConversionError(TypePtr(left_type->copy()), TypePtr(right_type->copy()), getLocation());
		} else {
			left->compile(left_var, function, context);
			right->compile(right_var, function, context);
		}

		function.add<SubRInstruction>(left_var, right_var, destination)->setDebug(*this);
	}
}

std::unique_ptr<Type> MinusExpr::getType(const Context &context) const {
	if (auto fnptr = getOperator(context))
		return std::unique_ptr<Type>(fnptr->returnType->copy());
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (left_type->isPointer() && right_type->isInt())
		return left_type;
	if (left_type->isPointer() && right_type->isPointer())
		return std::make_unique<SignedType>(64);
	if (!(*left_type && *right_type) || !(*right_type && *left_type))
		throw ImplicitConversionError(*left_type, *right_type, getLocation());
	return left_type;
}

void MultExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr left_var = function.newVar(), right_var = function.newVar();
		left->compile(left_var, function, context, 1);
		right->compile(right_var, function, context, multiplier); // TODO: verify
		function.add<MultRInstruction>(left_var, right_var, destination)->setDebug(*this);
	}
}

void ShiftLeftExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr temp_var = function.newVar();
		left->compile(temp_var, function, context, multiplier);
		right->compile(destination, function, context);
		function.add<ShiftLeftLogicalRInstruction>(temp_var, destination, destination)->setDebug(*this);
	}
}

size_t ShiftLeftExpr::getSize(const Context &context) const {
	return left->getSize(context);
}

std::optional<ssize_t> ShiftLeftExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value)
		return *left_value << *right_value;
	return std::nullopt;
}

void ShiftRightExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr temp_var = function.newVar();
		left->compile(temp_var, function, context, multiplier);
		right->compile(destination, function, context);
		if (left->getType(context)->isUnsigned())
			function.add<ShiftRightLogicalRInstruction>(temp_var, destination, destination)->setDebug(*this);
		else
			function.add<ShiftRightArithmeticRInstruction>(temp_var, destination, destination)->setDebug(*this);
	}
}

size_t ShiftRightExpr::getSize(const Context &context) const {
	return left->getSize(context);
}

std::optional<ssize_t> ShiftRightExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value) {
		if (left->getType(context)->isUnsigned())
			return size_t(*left_value) << *right_value;
		return *left_value << *right_value;
	}
	return std::nullopt;
}

void AndExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr temp_var = function.newVar();
		left->compile(temp_var, function, context);
		right->compile(destination, function, context);
		function.add<AndRInstruction>(temp_var, destination, destination)->setDebug(*this);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

size_t AndExpr::getSize(const Context &context) const {
	return left->getSize(context);
}

std::optional<ssize_t> AndExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value)
		return *left_value & *right_value;
	return std::nullopt;
}

void OrExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr temp_var = function.newVar();
		left->compile(temp_var, function, context);
		right->compile(destination, function, context);
		function.add<OrRInstruction>(temp_var, destination, destination)->setDebug(*this);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

size_t OrExpr::getSize(const Context &context) const {
	return left->getSize(context);
}

std::optional<ssize_t> OrExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value)
		return *left_value | *right_value;
	return std::nullopt;
}

void XorExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr temp_var = function.newVar();
		left->compile(temp_var, function, context);
		right->compile(destination, function, context);
		function.add<XorRInstruction>(temp_var, destination, destination)->setDebug(*this);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

size_t XorExpr::getSize(const Context &context) const {
	return left->getSize(context);
}

std::optional<ssize_t> XorExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value)
		return *left_value ^ *right_value;
	return std::nullopt;
}

void LandExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		const std::string base = "." + function.mangle() + "." + std::to_string(function.getNextBlock());
		const std::string success = base + "land.s", end = base + "land.e";
		left->compile(destination, function, context);
		function.add<JumpConditionalInstruction>(success, destination, false)->setDebug(*this);
		function.add<JumpInstruction>(end)->setDebug(*this);
		function.add<Label>(success);
		right->compile(destination, function, context);
		function.add<Label>(end);

		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

std::optional<ssize_t> LandExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value)
		return *left_value && *right_value;
	return std::nullopt;
}

void LorExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		const std::string success = "." + function.mangle() + "." + std::to_string(function.getNextBlock()) + "lor.s";
		left->compile(destination, function, context);
		function.add<JumpConditionalInstruction>(success, destination, false)->setDebug(*this);
		right->compile(destination, function, context);
		function.add<Label>(success);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

std::optional<ssize_t> LorExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value)
		return *left_value || *right_value;
	return std::nullopt;
}

void LxorExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		auto temp_var = function.newVar();
		left->compile(destination, function, context);
		right->compile(temp_var, function, context);
		function.add<LxorRInstruction>(destination, temp_var, destination)->setDebug(*this);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

std::optional<ssize_t> LxorExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value)
		return *left_value || *right_value;
	return std::nullopt;
}

void DivExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr temp_var = function.newVar();
		left->compile(temp_var, function, context, multiplier);
		right->compile(destination, function, context);
		if (left->getType(context)->isUnsigned())
			function.add<DivuRInstruction>(temp_var, destination, destination)->setDebug(*this);
		else
			function.add<DivRInstruction>(temp_var, destination, destination)->setDebug(*this);
	}
}

size_t DivExpr::getSize(const Context &context) const {
	return left->getSize(context);
}

std::optional<ssize_t> DivExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value) {
		if (left->getType(context)->isUnsigned())
			return size_t(*left_value) / size_t(*right_value);
		return *left_value / *right_value;
	}
	return std::nullopt;
}

void ModExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (auto fnptr = function.program.getOperator({left_type.get(), right_type.get()}, CPMTOK_MOD, getLocation())) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		VregPtr temp_var = function.newVar();
		left->compile(temp_var, function, context, multiplier);
		right->compile(destination, function, context);
		if (left->getType(context)->isUnsigned())
			function.add<ModuRInstruction>(temp_var, destination, destination)->setDebug(*this);
		else
			function.add<ModRInstruction>(temp_var, destination, destination)->setDebug(*this);
	}
}

size_t ModExpr::getSize(const Context &context) const {
	return left->getSize(context);
}

std::optional<ssize_t> ModExpr::evaluate(const Context &context) const {
	auto left_value  = left?  left->evaluate(context)  : std::nullopt,
	     right_value = right? right->evaluate(context) : std::nullopt;
	if (left_value && right_value) {
		if (left->getType(context)->isUnsigned())
			return size_t(*left_value) % size_t(*right_value);
		return *left_value % *right_value;
	}
	return std::nullopt;
}

ssize_t NumberExpr::getValue() const {
	getSize();
	return Util::parseLong(literal.substr(0, literal.find_first_of("su")));
}

void NumberExpr::compile(VregPtr destination, Function &function, const Context &, ssize_t multiplier) {
	const ssize_t multiplied = getValue() * multiplier;
	getSize();
	if (!destination)
		return;
	if (Util::inRange(multiplied)) {
		function.add<SetIInstruction>(destination, int(multiplied))->setDebug(*this);
	} else {
		const size_t high = size_t(multiplied) >> 32;
		const size_t low  = size_t(multiplied) & 0xff'ff'ff'ff;
		function.add<SetIInstruction>(destination, int(low))->setDebug(*this);
		function.add<LuiIInstruction>(destination, int(high))->setDebug(*this);
	}
}

size_t NumberExpr::getSize(const Context &) const {
	return getSize();
}

size_t NumberExpr::getSize() const {
	const size_t suffix = literal.find_first_of("su");

	// TODO: bounds checking for unsigned literals

	if (suffix == std::string::npos)
		return 8;

	const ssize_t value = Util::parseLong(literal.substr(0, suffix));
	const size_t size = Util::parseLong(literal.substr(suffix + 1)) / 8;
	bool out_of_range = false;

	if (literal.find('u') != std::string::npos)
		switch (size) {
			case 8:
				break;
			case 4:
				out_of_range = UINT_MAX < value;
				break;
			case 2:
				out_of_range = 65535 < value;
				break;
			case 1:
				out_of_range = 255 < value;
				break;
			default:
				throw GenericError(getLocation(), "Invalid numeric literal size (in bytes): " + std::to_string(size));
		}
	else
		switch (size) {
			case 8:
				break;
			case 4:
				out_of_range = value < INT_MIN || INT_MAX < value;
				break;
			case 2:
				out_of_range = value < -32768 || 32767 < value;
				break;
			case 1:
				out_of_range = value < -128 || 127 < value;
				break;
			default:
				throw GenericError(getLocation(), "Invalid numeric literal size (in bytes): " + std::to_string(size));
		}

	if (out_of_range)
		throw GenericError(getLocation(), "Numeric literal cannot fit in " + std::to_string(size) + " byte" +
			(size == 1? "" : "s") + ": " + std::to_string(value));

	return size;
}

std::unique_ptr<Type> NumberExpr::getType(const Context &context) const {
	const size_t bits = getSize(context) * 8;
	if (literal.find('u') != std::string::npos)
		return std::make_unique<UnsignedType>(bits);
	return std::make_unique<SignedType>(bits);
}

void BoolExpr::compile(VregPtr destination, Function &function, const Context &, ssize_t multiplier) {
	if (destination)
		function.add<SetIInstruction>(destination, value? size_t(multiplier) : 0)->setDebug(*this);
}

void NullExpr::compile(VregPtr destination, Function &function, const Context &, ssize_t) {
	if (destination)
		function.add<SetIInstruction>(destination, 0)->setDebug(*this);
}

void VregExpr::compile(VregPtr destination, Function &function, const Context &, ssize_t multiplier) {
	if (destination != virtualRegister) {
		if (multiplier == 1)
			function.add<MoveInstruction>(virtualRegister, destination)->setDebug(*this);
		else
			function.add<MultIInstruction>(virtualRegister, destination, size_t(multiplier))->setDebug(*this);
	}
}

size_t VregExpr::getSize(const Context &) const {
	if (auto vreg_type = virtualRegister->getType())
		return vreg_type->getSize();
	return 8;
}

std::unique_ptr<Type> VregExpr::getType(const Context &) const {
	if (auto vreg_type = virtualRegister->getType())
		return std::unique_ptr<Type>(vreg_type->copy());
	return nullptr;
}

void VariableExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (VariablePtr var = context.scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var)) {
			function.add<LoadIInstruction>(destination, global->name, global->getSize())->setDebug(*this);
		} else if (function.stackOffsets.count(var) == 0) {
			throw NotOnStackError(var);
		} else {
			const size_t offset = function.stackOffsets.at(var);
			function.addComment("Load variable " + name);
			function.add<SubIInstruction>(function.precolored(Why::framePointerOffset), destination, offset)
				->setDebug(*this);
			function.add<LoadRInstruction>(destination, destination, var->getSize())->setDebug(*this);
		}
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
		return;
	} else if (const auto fn = context.scope->lookupFunction(name, getLocation())) {
		function.add<SetIInstruction>(destination, fn->mangle())->setDebug(*this);
	} else
		throw ResolutionError(name, context.scope, getLocation());
}

bool VariableExpr::compileAddress(VregPtr destination, Function &function, const Context &context) {
	if (VariablePtr var = context.scope->lookup(name)) {
		if (auto global = std::dynamic_pointer_cast<Global>(var)) {
			function.add<SetIInstruction>(destination, global->name)->setDebug(*this);
		} else if (function.stackOffsets.count(var) == 0) {
			throw NotOnStackError(var, getLocation());
		} else {
			const size_t offset = function.stackOffsets.at(var);
			function.addComment("Get variable lvalue for " + name);
			function.add<SubIInstruction>(function.precolored(Why::framePointerOffset), destination, offset)
				->setDebug(*this);
			if (var->getType()->isReference()) {
				function.addComment("Load reference lvalue for " + name);
				function.add<LoadRInstruction>(destination, destination, Why::wordSize)->setDebug(*this);
			}
		}
	} else if (const auto fn = context.scope->lookupFunction(name, getLocation())) {
		function.add<SetIInstruction>(destination, fn->mangle())->setDebug(*this);
	} else
		throw ResolutionError(name, context.scope, getLocation());
	return true;
}

size_t VariableExpr::getSize(const Context &context) const {
	if (VariablePtr var = context.scope->lookup(name)) {
		return var->getSize();
	} else if (context.scope->lookupFunction(name, nullptr, {}, "", getLocation()))
		return Why::wordSize;
	throw ResolutionError(name, context, getLocation());
}

std::unique_ptr<Type> VariableExpr::getType(const Context &context) const {
	if (VariablePtr var = context.scope->lookup(name))
		return std::unique_ptr<Type>(var->getType()->copy()->setLvalue(true));
	try {
		if (const auto fn = context.scope->lookupFunction(name, nullptr, {}, context.structName, getLocation()))
			return std::make_unique<FunctionPointerType>(*fn);
	} catch (AmbiguousError &) {}
	throw ResolutionError(name, context, getLocation());
}

void AddressOfExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (multiplier != 1)
		throw GenericError(getLocation(), "Cannot multiply in AddressOfExpr");

	if (!destination)
		return;

	if (!subexpr->compileAddress(destination, function, context))
		throw LvalueError(*subexpr);
}

std::unique_ptr<Type> AddressOfExpr::getType(const Context &context) const {
	if (!subexpr->isLvalue(context))
		throw LvalueError(*subexpr);
	auto subexpr_type = subexpr->getType(context);
	auto out = std::make_unique<PointerType>(subexpr_type->copy());
	out->subtype->setConst(subexpr_type->isConst);
	return out;
}

void NotExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	auto type = subexpr->getType(context);
	if (auto fnptr = function.program.getOperator({type.get()}, CPMTOK_TILDE, getLocation())) {
		compileCall(destination, function, context, fnptr, {subexpr.get()}, getLocation(), multiplier);
	} else {
		subexpr->compile(destination, function, context);
		function.add<NotRInstruction>(destination, destination)->setDebug(*this);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

std::unique_ptr<Type> NotExpr::getType(const Context &context) const {
	auto type = subexpr->getType(context);
	if (auto fnptr = context.program->getOperator({type.get()}, CPMTOK_TILDE, getLocation()))
		return std::unique_ptr<Type>(fnptr->returnType->copy());
	return type;
}

void LnotExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	auto type = subexpr->getType(context);
	if (auto fnptr = function.program.getOperator({type.get()}, CPMTOK_NOT, getLocation())) {
		compileCall(destination, function, context, fnptr, {subexpr.get()}, getLocation(), multiplier);
	} else {
		subexpr->compile(destination, function, context);
		function.add<LnotRInstruction>(destination, destination)->setDebug(*this);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

std::unique_ptr<Type> LnotExpr::getType(const Context &context) const {
	auto type = subexpr->getType(context);
	if (auto fnptr = context.program->getOperator({type.get()}, CPMTOK_NOT, getLocation()))
		return std::unique_ptr<Type>(fnptr->returnType->copy());
	return std::make_unique<BoolType>();
}

void StringExpr::compile(VregPtr destination, Function &function, const Context &, ssize_t multiplier) {
	if (multiplier != 1)
		throw GenericError(getLocation(), "Cannot multiply in StringExpr");
	if (destination)
		function.add<SetIInstruction>(destination, getID(function.program))->setDebug(*this);
}

std::unique_ptr<Type> StringExpr::getType(const Context &) const {
	return std::make_unique<PointerType>((new UnsignedType(8))->setConst(true));
}

std::string StringExpr::getID(Program &program) const {
	return ".str" + std::to_string(program.getStringID(contents));
}

void DerefExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		compileCall(destination, function, context, fnptr, {subexpr.get()}, getLocation(), multiplier);
	} else {
		checkType(context);
		subexpr->compile(destination, function, context, multiplier);
		function.add<LoadRInstruction>(destination, destination, getSize(context))->setDebug(*this);
	}
}

size_t DerefExpr::getSize(const Context &context) const {
	auto type = checkType(context);
	return dynamic_cast<PointerType &>(*type).subtype->getSize();
}

std::unique_ptr<Type> DerefExpr::getType(const Context &context) const {
	if (auto fnptr = getOperator(context))
		return std::unique_ptr<Type>(fnptr->returnType->copy()->setLvalue(true));
	auto type = checkType(context);
	auto out = std::make_unique<ReferenceType>(dynamic_cast<PointerType &>(*type).subtype->copy());
	out->subtype->setConst(type->isConst);
	out->setLvalue(true);
	return out;
}

std::unique_ptr<Type> DerefExpr::checkType(const Context &context) const {
	auto type = subexpr->getType(context);
	if (!type->isPointer()) {
		if (auto *ref_type = type->cast<ReferenceType>())
			if (ref_type->subtype->isPointer())
				return std::unique_ptr<Type>(ref_type->subtype->copy());
		throw NotPointerError(TypePtr(type->copy()), getLocation());
	}
	return type;
}

bool DerefExpr::compileAddress(VregPtr destination, Function &function, const Context &context) {
	checkType(context);
	subexpr->compile(destination, function, context, 1);
	return true;
}

FunctionPtr DerefExpr::getOperator(const Context &context) const {
	auto subexpr_type = subexpr->getType(context);
	return context.program->getOperator({subexpr_type.get()}, CPMTOK_TIMES, getLocation());
}

Expr * CallExpr::copy() const {
	std::vector<ExprPtr> arguments_copy;
	for (const ExprPtr &argument: arguments)
		arguments_copy.emplace_back(argument->copy());
	auto *out = new CallExpr(subexpr->copy(), std::move(arguments_copy));
	out->setDebug(debug);
	out->structName = structName;
	if (structExpr)
		out->structExpr = std::unique_ptr<Expr>(structExpr->copy());
	return out;
}

void CallExpr::compile(VregPtr destination, Function &fn, const Context &context, ssize_t multiplier) {
	Context subcontext(context);
	subcontext.structName = getStructName(context);

	if (subcontext.structName.empty()) {
		if (auto fnptr = getOperator(subcontext)) {
			std::vector<ExprPtr> call_exprs {subexpr};
			call_exprs.reserve(1 + arguments.size());
			for (const auto &argument: arguments)
				call_exprs.push_back(argument);
			std::vector<Argument> call_args;
			call_args.reserve(1 + arguments.size());
			for (const auto &expr: call_exprs)
				call_args.push_back(expr.get());
			compileCall(destination, fn, subcontext, fnptr, call_args, getLocation(), multiplier);
			return;
		}
	}

	std::function<const Type &(size_t)> get_arg_type = [this](size_t) -> const Type & {
		throw GenericError(getLocation(), "get_arg_type not redefined");
	};

	std::function<void()> add_jump = [this] {
		throw GenericError(getLocation(), "add_jump not redefined");
	};

	TypePtr found_return_type = nullptr;

	bool function_found = false;
	int argument_offset = Why::argumentOffset;

	std::unique_ptr<Type> struct_expr_type;

	const size_t registers_used = (structExpr? 1 : 0) + arguments.size();
	if (Why::argumentCount < registers_used)
		throw std::runtime_error("Functions with more than 16 arguments aren't currently supported.");

	fn.add<CallPushPlaceholder>()->setDebug(*this);

	for (size_t i = 0; i < registers_used; ++i)
		fn.add<StackPushInstruction>(fn.precolored(Why::argumentOffset + i))->setDebug(*this);

	if (structExpr) {
		struct_expr_type = structExpr->getType(subcontext);
		auto this_var = fn.precolored(argument_offset++);
		if (!struct_expr_type->isStruct()) {
			if (const auto *pointer_type = struct_expr_type->cast<PointerType>()) {
				if (pointer_type->subtype->isStruct()) {
					fn.addComment("Setting \"this\" from pointer.");
					this_var->setType(*struct_expr_type);
					structExpr->compile(this_var, fn, context);
					goto this_done; // Hehe :)
				}
			} else if (const auto *reference_type = struct_expr_type->cast<ReferenceType>()) {
				if (reference_type->subtype->isStruct()) {
					fn.addComment("Setting \"this\" from reference.");
					this_var->setType(*struct_expr_type);
					structExpr->compileAddress(this_var, fn, context);
					goto this_done; // Haha :)
				}
			}
			throw NotStructError(TypePtr(struct_expr_type->copy()), structExpr->getLocation());
		} else {
			fn.addComment("Setting \"this\" from struct of type " + std::string(*struct_expr_type) + ".");
			this_var->setType(PointerType(struct_expr_type->copy()));
			if (!structExpr->compileAddress(this_var, fn, context))
				throw LvalueError(*struct_expr_type, structExpr->getLocation());
		}
		this_done:
		fn.addComment("Done setting \"this\".");
	}

	FunctionPtr found;

	if (auto *var_expr = subexpr->cast<VariableExpr>())
		found = findFunction(var_expr->name, subcontext);

	if (found) {
		if (found->argumentCount() != arguments.size())
			throw GenericError(getLocation(), "Invalid number of arguments in call to " + found->name + " at " +
				std::string(getLocation()) + ": " + std::to_string(arguments.size()) + " (expected " +
				std::to_string(found->argumentCount()) + ")");

		const bool special = found->name == "$c" || found->name == "$d";
		if (!special && struct_expr_type && struct_expr_type->isConst && !found->isConst())
			throw ConstError("Can't call non-const method " + subcontext.structName + "::" + found->name,
				*struct_expr_type, getLocation());

		function_found = true;
		found_return_type = found->returnType;
		get_arg_type = [found](size_t i) -> const Type & { return *found->getArgumentType(i); };
		add_jump = [this, found, &fn] { fn.add<JumpInstruction>(found->mangle(), true)->setDebug(*this); };
	}

	std::unique_ptr<Type> fnptr_type;

	if (!function_found) {
		fnptr_type = subexpr->getType(subcontext);
		if (!fnptr_type->isFunctionPointer())
			throw FunctionPointerError(*fnptr_type);
		const auto *subfn = fnptr_type->cast<FunctionPointerType>();
		found_return_type = TypePtr(subfn->returnType->copy());
		get_arg_type = [subfn](size_t i) -> const Type & { return *subfn->argumentTypes.at(i); };
		add_jump = [this, &fn, &context] {
			auto jump_destination = fn.newVar();
			subexpr->compile(jump_destination, fn, context);
			fn.add<JumpRegisterInstruction>(jump_destination, true)->setDebug(*this);;
		};
	}

	size_t i = 0;

	for (const auto &argument: arguments) {
		auto argument_register = fn.precolored(argument_offset + i);
		auto argument_type = argument->getType(subcontext);
		const Type &function_argument_type = get_arg_type(i);
		if (function_argument_type.isReference()) {
			fn.addComment("CallExpr::compile: compiling address into reference argument");
			if (!argument->compileAddress(argument_register, fn, context))
				throw LvalueError(*argument_type, argument->getLocation());
		} else if (argument_type->isStruct()) {
			throw GenericError(argument->getLocation(),
				"Structs cannot be directly passed to functions; use a pointer");
		} else
			argument->compile(argument_register, fn, context);
		try {
			typeCheck(*argument_type, function_argument_type, argument_register, fn, argument->getLocation());
		} catch (ImplicitConversionError &) {
			std::cerr << "\e[31mBad function argument at " << argument->getLocation() << "\e[39m\n";
			throw;
		}
		++i;
	}

	add_jump();

	for (size_t i = registers_used; 0 < i; --i)
		fn.add<StackPopInstruction>(fn.precolored(Why::argumentOffset + i - 1))->setDebug(*this);

	fn.add<CallPopPlaceholder>();

	if (!found_return_type->isVoid() && destination) {
		if (multiplier == 1)
			fn.add<MoveInstruction>(fn.precolored(Why::returnValueOffset), destination)->setDebug(*this);
		else
			fn.add<MultIInstruction>(fn.precolored(Why::returnValueOffset), destination, size_t(multiplier))
				->setDebug(*this);
	}
}

TypePtr CallExpr::getReturnType(const Context &context) const {
	Context subcontext(context);
	subcontext.structName = getStructName(context);

	FunctionPtr found;
	TypePtr found_return_type;

	if (auto *var_expr = subexpr->cast<VariableExpr>())
		if (auto found = findFunction(var_expr->name, subcontext))
			return found->returnType;

	auto fnptr_type = subexpr->getType(subcontext);
	if (!fnptr_type->isFunctionPointer())
		throw FunctionPointerError(*fnptr_type);
	const auto *subfn = fnptr_type->cast<FunctionPointerType>();
	return TypePtr(subfn->returnType->copy());
}

bool CallExpr::compileAddress(VregPtr destination, Function &function, const Context &context) {
	if (isLvalue(context)) {
		function.addComment("Starting Call::compileAddress");
		compile(destination, function, context, 1);
		function.addComment("Finished Call::compileAddress");
		return true;
	}

	return false;
}

bool CallExpr::isLvalue(const Context &context) const {
	return getReturnType(context)->isReference();
}

CallExpr::operator std::string() const {
	std::stringstream out;
	if (!structName.empty())
		out << structName << "::";
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
	Context subcontext(context);
	subcontext.structName = getStructName(context);
	if (auto fnptr = getOperator(subcontext))
		return std::unique_ptr<Type>(fnptr->returnType->copy());
	if (auto *var_expr = subexpr->cast<VariableExpr>()) {
		if (const auto fn = findFunction(var_expr->name, subcontext))
			return std::unique_ptr<Type>(fn->returnType->copy());
		if (auto var = subcontext.scope->lookup(var_expr->name)) {
			if (auto *fnptr = var->getType()->cast<FunctionPointerType>())
				return std::unique_ptr<Type>(fnptr->returnType->copy());
			throw FunctionPointerError(*var->getType());
		}
		throw ResolutionError(var_expr->name, subcontext, getLocation());
	}
	auto type = subexpr->getType(subcontext);
	if (const auto *fnptr = type->cast<FunctionPointerType>())
		return std::unique_ptr<Type>(fnptr->returnType->copy());
	throw FunctionPointerError(*type);
}

FunctionPtr HasArguments::findFunction(const std::string &name, const Context &context) const {
	Types arg_types;
	arg_types.reserve(arguments.size());
	for (const auto &expr: arguments)
		arg_types.push_back(expr->getType(context));
	return context.scope->lookupFunction(name, arg_types, context.structName, getLocation());
}

std::string CallExpr::getStructName(const Context &context) const {
	if (structExpr) {
		auto type = structExpr->getType(context);

		if (const auto *struct_type = type->cast<StructType>())
			return struct_type->name;

		if (const auto *pointer_type = type->cast<PointerType>())
			if (const auto *struct_type = pointer_type->subtype->cast<StructType>())
				return struct_type->name;

		if (const auto *reference_type = type->cast<ReferenceType>())
			if (const auto *struct_type = reference_type->subtype->cast<StructType>())
				return struct_type->name;

		throw NotStructError(std::move(type), structExpr->getLocation());
	}

	return structName;
}

FunctionPtr CallExpr::getOperator(const Context &context) const {
	try {
		std::vector<std::unique_ptr<Type>> unique_types;
		unique_types.push_back(subexpr->getType(context));
		for (const auto &argument: arguments)
			unique_types.push_back(argument->getType(context));
		std::vector<Type *> types;
		for (auto &unique_type: unique_types)
			types.push_back(unique_type.get());
		return context.program->getOperator(types, CPMTOK_LPAREN, getLocation());
	} catch (ResolutionError &) {
		return nullptr;
	} catch (AmbiguousError &) {
		return nullptr;
	}
}

void AssignExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	TypePtr left_type = left->getType(context), right_type = right->getType(context);
	if (left_type->isConst)
		throw ConstError("Can't assign", *left_type, getLocation());
	if (auto fnptr = function.program.getOperator({left_type.get(), right_type.get()}, CPMTOK_ASSIGN, getLocation())) {
		compileCall(destination, function, context, fnptr, {left.get(), right.get()}, getLocation(), multiplier);
	} else {
		auto addr_var = function.newVar();
		if (!destination)
			destination = function.newVar();
		TypePtr right_type = right->getType(context);
		if (!left->compileAddress(addr_var, function, context))
			throw LvalueError(*left->getType(context));
		if (right_type->isInitializer()) {
			auto *initializer_expr = right->cast<InitializerExpr>();
			if (initializer_expr->isConstructor) {
				StructType *struct_type;
				if (left_type->isStruct()) {
					struct_type = left_type->cast<StructType>();
				} else if (left_type->isReference()) {
					auto left_reference = left_type->ptrcast<ReferenceType>();
					if (left_reference->subtype->isStruct())
						struct_type = left_reference->subtype->cast<StructType>();
					else
						throw NotStructError(left_type, left->getLocation());
				} else
					throw NotStructError(left_type, left->getLocation());
				auto *constructor_expr = new VariableExpr("$c");
				auto call = std::make_unique<CallExpr>(constructor_expr, initializer_expr->children);
				call->debug = debug;
				call->structExpr = std::unique_ptr<Expr>(left->copy());
				call->structExpr->debug = debug;
				function.addComment("Calling constructor for " + std::string(*struct_type));
				call->compile(nullptr, function, context, 1);
			} else {
				if (!tryCast(*right_type, *left_type, nullptr, function, getLocation()))
					throw ImplicitConversionError(right_type, left_type, getLocation());
				initializer_expr->fullCompile(addr_var, function, context);
			}
		} else {
			right->compile(destination, function, context);
			if (!tryCast(*right_type, *left_type, destination, function, getLocation()))
				throw ImplicitConversionError(right_type, left_type, getLocation());
			function.add<StoreRInstruction>(destination, addr_var, getSize(context))->setDebug(*this);
			if (multiplier != 1)
				function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
		}
	}
}

std::unique_ptr<Type> AssignExpr::getType(const Context &context) const {
	if (auto fnptr = getOperator(context))
		return std::unique_ptr<Type>(fnptr->returnType->copy());
	auto left_type = left->getType(context), right_type = right->getType(context);
	if (!(*right_type && *left_type))
			throw ImplicitConversionError(*right_type, *left_type, getLocation());
	return left_type;
}

std::optional<ssize_t> AssignExpr::evaluate(const Context &context) const {
	return right? right->evaluate(context) : std::nullopt;
}

bool AssignExpr::compileAddress(VregPtr destination, Function &function, const Context &context) {
	return left? left->compileAddress(destination, function, context) : false;
}

void CastExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	// TODO: operator overloading
	subexpr->compile(destination, function, context, multiplier);
	tryCast(*subexpr->getType(context), *targetType, destination, function, getLocation());
}

std::unique_ptr<Type> CastExpr::getType(const Context &) const {
	return std::unique_ptr<Type>(targetType->copy());
}

void AccessExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (auto fnptr = getOperator(context)) {
		// TODO: verify both pointers and arrays
		compileCall(destination, function, context, fnptr, {array.get(), subscript.get()}, getLocation(), multiplier);
	} else {
		TypePtr array_type = array->getType(context);
		if (auto casted = array_type->ptrcast<ArrayType>())
			array_type = PointerType::make(casted->subtype->copy());
		else if (!array_type->is<PointerType>())
			fail();
		compileAddress(destination, function, context);
		function.add<LoadRInstruction>(destination, destination, getSize(context))->setDebug(*this);
		if (multiplier != 1)
			function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
	}
}

std::unique_ptr<Type> AccessExpr::getType(const Context &context) const {
	if (auto fnptr = getOperator(context))
		return std::unique_ptr<Type>(fnptr->returnType->copy());
	auto array_type = array->getType(context);
	if (auto *casted = array_type->cast<const ArrayType>())
		return std::unique_ptr<Type>(casted->subtype->copy());
	if (auto *casted = array_type->cast<const PointerType>())
		return std::unique_ptr<Type>(casted->subtype->copy());
	fail();
	return nullptr;
}

void AccessExpr::fail() const {
	throw GenericError(getLocation(), "Can't get array access result type: array expression isn't an array or pointer "
		"type");
}

bool AccessExpr::compileAddress(VregPtr destination, Function &function, const Context &context) {
	if (check(context)->isPointer())
		array->compile(destination, function, context);
	else if (!array->compileAddress(destination, function, context))
		throw LvalueError(std::string(*array->getType(context)));
	const auto element_size = getSize(context);
	const auto subscript_value = subscript->evaluate(context);
	if (subscript_value) {
		if (*subscript_value != 0)
			function.add<AddIInstruction>(destination, destination, size_t(*subscript_value * element_size))
				->setDebug(*this);
	} else {
		auto subscript_variable = function.newVar();
		subscript->compile(subscript_variable, function, context);
		if (element_size != 1)
			function.add<MultIInstruction>(subscript_variable, subscript_variable, element_size)->setDebug(*this);
		function.add<AddRInstruction>(destination, subscript_variable, destination)->setDebug(*this);
	}
	return true;
}

std::unique_ptr<Type> AccessExpr::check(const Context &context) {
	auto type = array->getType(context);
	const bool is_array = type->isArray(), is_pointer = type->isPointer();
	if (!is_array && !is_pointer)
		throw NotArrayError(TypePtr(type->copy()));
	if (!warned && is_array)
		if (const auto evaluated = subscript->evaluate(context)) {
			const auto array_count = type->cast<ArrayType>()->count;
			if (*evaluated < 0 || ssize_t(array_count) <= *evaluated) {
				warn() << "Array index " << *evaluated << " at " << subscript->getLocation()
				       << " is higher than array size (" << array_count << ") at " << getLocation() << '\n';
				warned = true;
			}
		}
	return std::unique_ptr<Type>(type->copy());
}

FunctionPtr AccessExpr::getOperator(const Context &context) const {
	auto array_type = array->getType(context), subscript_type = subscript->getType(context);
	return context.program->getOperator({array_type.get(), subscript_type.get()}, CPM_ACCESS, getLocation());
}

void LengthExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	TypePtr type = subexpr->getType(context);
	if (auto fnptr = function.program.getOperator({type.get()}, CPMTOK_HASH, getLocation())) {
		compileCall(destination, function, context, fnptr, {subexpr.get()}, getLocation(), multiplier);
	} else if (auto *array = type->cast<const ArrayType>()) {
		function.add<SetIInstruction>(destination, array->count * multiplier)->setDebug(*this);
	} else
		throw NotArrayError(type);
}

void TernaryExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	const std::string base = "." + function.mangle() + "." + std::to_string(function.getNextBlock());
	const std::string true_label = base + "t.t", end = base + "t.e";
	condition->compile(destination, function, context);
	function.add<JumpConditionalInstruction>(true_label, destination, false)->setDebug(*this);
	ifFalse->compile(destination, function, context, multiplier);
	function.add<JumpInstruction>(end)->setDebug(*this);
	function.add<Label>(true_label);
	ifTrue->compile(destination, function, context, multiplier);
	function.add<Label>(end);
}

std::unique_ptr<Type> TernaryExpr::getType(const Context &context) const {
	auto condition_type = condition->getType(context);
	if (!(*condition_type && BoolType()))
		throw ImplicitConversionError(*condition_type, BoolType(), getLocation());
	auto true_type = ifTrue->getType(context), false_type = ifFalse->getType(context);
	if (!(*true_type && *false_type) || !(*false_type && *true_type))
		throw ImplicitConversionError(*false_type, *true_type, getLocation());
	return true_type;
}

size_t TernaryExpr::getSize(const Context &context) const {
	return getType(context)->getSize();
}

void DotExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	auto struct_type = checkType(context);
	const size_t field_size = struct_type->getFieldSize(ident);
	const size_t field_offset = struct_type->getFieldOffset(ident);
	Util::validateSize(field_size);
	auto left_type = left->getType(context);
	if (!left->compileAddress(destination, function, context))
		throw LvalueError(*left);
	if (field_offset != 0) {
		function.addComment("Add dot field offset of " + struct_type->name + "::" + ident);
		function.add<AddIInstruction>(destination, destination, field_offset)->setDebug(*this);
	} else
		function.addComment("Dot field offset of " + struct_type->name + "::" + ident + " is 0");
	function.addComment("Load dot field " + struct_type->name + "::" + ident);
	function.add<LoadRInstruction>(destination, destination, field_size)->setDebug(*this);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
}

std::unique_ptr<Type> DotExpr::getType(const Context &context) const {
	auto struct_type = checkType(context);
	const auto &map = struct_type->getMap();
	if (map.count(ident) == 0)
		throw ResolutionError(ident, context.scope, getLocation());
	auto out = std::unique_ptr<Type>(map.at(ident)->copy());
	if (struct_type->isConst)
		out->setConst(true);
	return out;
}

bool DotExpr::compileAddress(VregPtr destination, Function &function, const Context &context) {
	auto struct_type = checkType(context);
	const size_t field_offset = struct_type->getFieldOffset(ident);
	if (!left->compileAddress(destination, function, context))
		throw LvalueError(*left);
	if (field_offset != 0)
		function.add<AddIInstruction>(destination, destination, field_offset)->setDebug(*this);
	return true;
}

std::shared_ptr<StructType> DotExpr::checkType(const Context &context) const {
	auto left_type = left->getType(context);
	if (!left_type->isStruct()) {
		auto *left_reference = left_type->cast<ReferenceType>();
		if (!left_reference || !left_reference->subtype->isStruct())
			throw NotStructError(std::move(left_type), getLocation());
		return std::shared_ptr<StructType>(left_reference->subtype->copy()->cast<StructType>());
	}
	TypePtr shared_type = std::move(left_type);
	return shared_type->ptrcast<StructType>();
}

size_t DotExpr::getSize(const Context &context) const {
	return checkType(context)->getMap().at(ident)->getSize();
}

void ArrowExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	// TODO: operator overloading?
	auto struct_type = checkType(context);
	const size_t field_size = struct_type->getFieldSize(ident);
	const size_t field_offset = struct_type->getFieldOffset(ident);
	Util::validateSize(field_size);
	left->compile(destination, function, context, 1);
	if (field_offset != 0) {
		function.addComment("Add arrow field offset of " + struct_type->name + "::" + ident);
		function.add<AddIInstruction>(destination, destination, field_offset)->setDebug(*this);
	} else
		function.addComment("Arrow field offset of " + struct_type->name + "::" + ident + " is 0");
	function.addComment("Load arrow field " + struct_type->name + "::" + ident);
	function.add<LoadRInstruction>(destination, destination, field_size)->setDebug(*this);
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier))->setDebug(*this);
}

std::unique_ptr<Type> ArrowExpr::getType(const Context &context) const {
	auto struct_type = checkType(context);
	const auto &map = struct_type->getMap();
	if (map.count(ident) == 0)
		throw ResolutionError(ident, context.scope, getLocation());
	auto out = std::unique_ptr<Type>(map.at(ident)->copy());
	if (struct_type->isConst)
		out->setConst(true);
	return out;
}

bool ArrowExpr::compileAddress(VregPtr destination, Function &function, const Context &context) {
	auto struct_type = checkType(context);
	const size_t field_offset = struct_type->getFieldOffset(ident);
	left->compile(destination, function, context);
	if (field_offset != 0) {
		function.addComment("Add field offset of " + struct_type->name + "::" + ident);
		function.add<AddIInstruction>(destination, destination, field_offset)->setDebug(*this);
	} else
		function.addComment("Field offset of " + struct_type->name + "::" + ident + " is 0");
	return true;
}

std::shared_ptr<StructType> ArrowExpr::checkType(const Context &context) const {
	auto left_type = left->getType(context);
	if (left_type->isReference())
		left_type = std::unique_ptr<Type>(left_type->cast<ReferenceType>()->subtype->copy());
	if (!left_type->isPointer())
		throw NotPointerError(std::move(left_type), getLocation());
	auto *left_pointer = left_type->cast<PointerType>();
	TypePtr shared_type = TypePtr(left_pointer->subtype->copy());
	shared_type->setConst(left_pointer->subtype->isConst);
	if (!left_pointer->subtype->isStruct())
		throw NotStructError(shared_type, getLocation());
	return shared_type->ptrcast<StructType>();
}

size_t ArrowExpr::getSize(const Context &context) const {
	return checkType(context)->getMap().at(ident)->getSize();
}

void SizeofExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	function.add<SetIInstruction>(destination, size_t(*evaluate(context) * multiplier));
}

std::optional<ssize_t> OffsetofExpr::evaluate(const Context &context) const {
	auto type = context.scope->lookupType(structName);
	StructType *struct_type;
	if (!type || !(struct_type = type->cast<StructType>()))
		throw GenericError(getLocation(), "Unknown or incomplete struct in offsetof expression: " + structName);
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
		throw GenericError(getLocation(), "Struct " + structName + " has no field " + fieldName);
	return offset;
}

void OffsetofExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	function.add<SetIInstruction>(destination, size_t(*evaluate(context) * multiplier))->setDebug(*this);
}

std::optional<ssize_t> SizeofMemberExpr::evaluate(const Context &context) const {
	auto type = context.scope->lookupType(structName);
	StructType *struct_type;
	if (!type || !(struct_type = type->cast<StructType>()))
		throw GenericError(getLocation(), "Unknown or incomplete struct in sizeof expression: " + structName);
	const auto &map = struct_type->getMap();
	if (map.count(fieldName) == 0)
		throw GenericError(getLocation(), "Struct " + structName + " has no field " + fieldName);
	return map.at(fieldName)->getSize();
}

void SizeofMemberExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	function.add<SetIInstruction>(destination, size_t(*evaluate(context) * multiplier))->setDebug(*this);
}

Expr * InitializerExpr::copy() const {
	std::vector<ExprPtr> children_copy;
	for (const auto &child: children)
		children_copy.emplace_back(child->copy());
	return (new InitializerExpr(children_copy, isConstructor))->setDebug(debug);
}

std::unique_ptr<Type> InitializerExpr::getType(const Context &context) const {
	return std::make_unique<InitializerType>(children, context);
}

void InitializerExpr::compile(VregPtr, Function &, const Context &, ssize_t) {
	throw GenericError(getLocation(), "Can't compile initializers directly");
}

void InitializerExpr::fullCompile(VregPtr address, Function &function, const Context &context) {
	if (isConstructor)
		throw GenericError(getLocation(), "Can't compile a constructor initializer as a non-constructor initializer");
	if (children.empty())
		return;
	auto temp_var = function.newVar();
	for (const auto &child: children) {
		auto child_type = child->getType(context);
		if (child_type->isInitializer()) {
			child->cast<InitializerExpr>()->fullCompile(address, function, context);
		} else {
			child->compile(temp_var, function, context);
			const size_t size = child->getSize(context);
			function.addComment("InitializerExpr: store and increment");
			function.add<StoreRInstruction>(temp_var, address, size)->setDebug(*this);
			function.add<AddIInstruction>(address, address, size)->setDebug(*this);
		}
	}
}

Expr * ConstructorExpr::copy() const {
	std::vector<ExprPtr> arguments_copy;
	for (const ExprPtr &argument: arguments)
		arguments_copy.emplace_back(argument->copy());
	return (new ConstructorExpr(stackOffset, structName, arguments_copy))->setDebug(debug);
}

void ConstructorExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (multiplier != 1)
		throw GenericError(getLocation(), "Cannot multiply in ConstructorExpr");

	Context subcontext(context);
	subcontext.structName = structName;
	int argument_offset = Why::argumentOffset;

	const size_t registers_used = 1 + arguments.size();
	if (Why::argumentCount < registers_used)
		throw std::runtime_error("Functions with more than 16 arguments aren't currently supported.");

	function.add<CallPushPlaceholder>();

	for (size_t i = 0; i < registers_used; ++i)
		function.add<StackPushInstruction>(function.precolored(Why::argumentOffset + i))->setDebug(*this);

	auto looked_up = subcontext.scope->lookupType(structName);
	if (!looked_up)
		throw ResolutionError(structName, subcontext, getLocation());

	auto struct_type = looked_up->ptrcast<StructType>();
	auto this_var = function.precolored(argument_offset++);
	function.addComment("Setting \"this\" for constructor.");
	function.add<SubIInstruction>(function.precolored(Why::framePointerOffset), this_var, stackOffset)->setDebug(*this);

	FunctionPtr found = findFunction(subcontext);

	if (!found)
		throw GenericError(getLocation(), "Constructor for " + structName + " not found.");

	if (found->argumentCount() != arguments.size())
		throw GenericError(getLocation(), "Invalid number of arguments in call to " + structName + " constructor at " +
			std::string(getLocation()) + ": " + std::to_string(arguments.size()) + " (expected " +
			std::to_string(found->argumentCount()) + ")");

	size_t i = 0;

	for (const auto &argument: arguments) {
		auto argument_register = function.precolored(argument_offset + i);
		auto argument_type = argument->getType(subcontext);
		const Type &function_argument_type = *found->getArgumentType(i);
		if (function_argument_type.isReference()) {
			if (!argument->compileAddress(argument_register, function, context))
				throw LvalueError(*argument_type, argument->getLocation());
		} else if (argument_type->isStruct()) {
			throw GenericError(argument->getLocation(),
				"Structs cannot be directly passed to functions; use a pointer");
		} else
			argument->compile(argument_register, function, context);
		try {
			typeCheck(*argument_type, *found->getArgumentType(i), argument_register, function, argument->getLocation());
		} catch (std::out_of_range &err) {
			std::cerr << "\e[31mBad function argument at " << argument->getLocation() << "\e[39m\n";
			throw;
		}
		++i;
	}

	function.add<JumpInstruction>(found->mangle(), true)->setDebug(*this);

	for (size_t i = registers_used; 0 < i; --i)
		function.add<StackPopInstruction>(function.precolored(Why::argumentOffset + i - 1))->setDebug(*this);

	function.add<CallPopPlaceholder>();

	if (!found->returnType->isVoid() && destination)
		function.add<MoveInstruction>(function.precolored(Why::returnValueOffset), destination)->setDebug(*this);
}

ConstructorExpr::operator std::string() const {
	std::stringstream out;
	out << '%' << structName << '(';
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

size_t ConstructorExpr::getSize(const Context &context) const {
	return getType(context)->getSize();
}

std::unique_ptr<Type> ConstructorExpr::getType(const Context &context) const {
	return std::make_unique<PointerType>(context.scope->lookupType(structName)->copy());
}

FunctionPtr ConstructorExpr::findFunction(const Context &context) const {
	return HasArguments::findFunction("$c", context);
}

static std::string newVariableName(const std::map<std::string, VariablePtr> &map) {
	for (size_t n = map.size();; ++n) {
		std::string variable_name = "!t" + std::to_string(n);
		if (map.count(variable_name) == 0)
			return variable_name;
	}
}

ConstructorExpr * ConstructorExpr::addToScope(const Context &context) {
	auto type = context.scope->lookupType(structName);
	if (!type)
		throw ResolutionError(structName, Context(*context.program, context.scope, structName), getLocation());

	if (auto *function_scope = context.scope->cast<FunctionScope>()) {
		Function &function = function_scope->function;
		const std::string variable_name = newVariableName(function.variables);
		auto variable = Variable::make(variable_name, type, function);
		function.variableOrder.push_back(variable);
		function.variables.emplace(variable_name, variable);
		function.stackOffsets.emplace(variable, stackOffset);
	} else if (auto *block_scope = context.scope->cast<BlockScope>()) {
		Function &function = block_scope->getFunction();
		const std::string variable_name = newVariableName(block_scope->variables);
		auto variable = Variable::make(variable_name, type, function);
		block_scope->variableOrder.push_back(variable);
		block_scope->variables.emplace(variable_name, variable);
		function.stackOffsets.emplace(variable, stackOffset);
	} else
		throw GenericError(getLocation(), "Invalid scope for ConstructorExpr: " + context.scope->partialStringify());

	return this;
}

Expr * NewExpr::copy() const {
	return (new NewExpr(type, arguments))->setDebug(debug);
}

void NewExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	if (multiplier != 1)
		throw GenericError(getLocation(), "Cannot multiply in NewExpr");

	Context subcontext(context);

	auto struct_type = type->ptrcast<StructType>();
	if (struct_type)
		subcontext.structName = struct_type->name;

	if (!destination)
		destination = function.newVar(getType(subcontext));

	auto call_expr = std::make_unique<CallExpr>(new VariableExpr("checked_malloc"),
		std::vector<ExprPtr> {SizeofExpr::make(type)});

	call_expr->debug = debug;
	call_expr->compile(destination, function, subcontext, 1);

	if (!struct_type) {
		if (1 < arguments.size())
			throw GenericError(getLocation(), "new operator for non-struct type cannot take multiple arguments");
		if (arguments.size() == 1) {
			auto temp_var = function.newVar();
			arguments.front()->compile(temp_var, function, subcontext, 1);
			function.add<StoreRInstruction>(temp_var, destination, type->getSize())->setDebug(*this);
		}
	} else {
		FunctionPtr found = findFunction(subcontext);
		if (!found)
			throw GenericError(getLocation(), "Constructor for " + struct_type->name + " not found.");

		int argument_offset = Why::argumentOffset;

		const size_t registers_used = 1 + arguments.size();
		if (Why::argumentCount < registers_used)
			throw std::runtime_error("Constructors with more than 15 arguments aren't currently supported.");

		function.add<CallPushPlaceholder>();

		for (size_t i = 0; i < registers_used; ++i)
			function.add<StackPushInstruction>(function.precolored(Why::argumentOffset + i))->setDebug(*this);

		auto this_var = function.precolored(argument_offset++);
		function.addComment("Moving \"this\" for constructor.");
		function.add<MoveInstruction>(destination, this_var)->setDebug(*this);

		if (found->argumentCount() != arguments.size())
			throw GenericError(getLocation(), "Invalid number of arguments in call to " + struct_type->name +
				" constructor at " + std::string(getLocation()) + ": " + std::to_string(arguments.size()) +
				" (expected " + std::to_string(found->argumentCount()) + ")");

		size_t i = 0;
		for (const auto &argument: arguments) {
			auto argument_register = function.precolored(argument_offset + i);
			auto argument_type = argument->getType(subcontext);
			const Type &function_argument_type = *found->getArgumentType(i);
			if (function_argument_type.isReference()) {
				if (!argument->compileAddress(argument_register, function, context))
					throw LvalueError(*argument_type, argument->getLocation());
			} else if (argument_type->isStruct()) {
				throw GenericError(argument->getLocation(),
					"Structs cannot be directly passed to functions; use a pointer");
			} else
				argument->compile(argument_register, function, context);
			try {
				typeCheck(*argument_type, *found->getArgumentType(i), argument_register, function,
					argument->getLocation());
			} catch (std::out_of_range &err) {
				error() << "Bad function argument at " << argument->getLocation() << '\n';
				throw;
			}
			++i;
		}

		function.add<JumpInstruction>(found->mangle(), true)->setDebug(*this);

		for (size_t i = registers_used; 0 < i; --i)
			function.add<StackPopInstruction>(function.precolored(Why::argumentOffset + i - 1))->setDebug(*this);

		function.add<CallPopPlaceholder>();

		function.add<MoveInstruction>(function.precolored(Why::returnValueOffset), destination)->setDebug(*this);
	}
}

NewExpr::operator std::string() const {
	std::stringstream out;
	out << "new " << *type << '(';
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

size_t NewExpr::getSize(const Context &) const {
	return type->getSize();
}

std::unique_ptr<Type> NewExpr::getType(const Context &) const {
	return std::make_unique<PointerType>(type->copy());
}

FunctionPtr NewExpr::findFunction(const Context &context) const {
	Types arg_types;
	arg_types.reserve(arguments.size());
	for (const auto &expr: arguments)
		arg_types.push_back(expr->getType(context));
	return context.scope->lookupFunction("$c", arg_types, context.structName, getLocation());
}

StaticFieldExpr::StaticFieldExpr(const std::string &struct_name, const std::string &field_name):
	structName(struct_name), fieldName(field_name) {}

Expr * StaticFieldExpr::copy() const {
	return (new StaticFieldExpr(structName, fieldName))->setDebug(debug);
}

void StaticFieldExpr::compile(VregPtr destination, Function &function, const Context &context, ssize_t multiplier) {
	function.add<LoadIInstruction>(destination, mangle(context), getSize(context));
	if (multiplier != 1)
		function.add<MultIInstruction>(destination, destination, size_t(multiplier));
}

bool StaticFieldExpr::compileAddress(VregPtr destination, Function &function, const Context &context) {
	function.add<SetIInstruction>(destination, mangle(context));
	return true;
}

StaticFieldExpr::operator std::string() const {
	return "%" + structName + "::" + fieldName;
}

size_t StaticFieldExpr::getSize(const Context &context) const {
	return getType(context)->getSize();
}

std::unique_ptr<Type> StaticFieldExpr::getType(const Context &context) const {
	const auto &statics = getStruct(context)->getStatics();
	if (statics.count(fieldName) == 0)
		throw ResolutionError(fieldName, Context(*context.program, context.scope, structName), getLocation());
	return std::unique_ptr<Type>(statics.at(fieldName)->copy());
}

std::shared_ptr<StructType> StaticFieldExpr::getStruct(const Context &context) const {
	auto type = context.scope->lookupType(structName);
	if (!type)
		throw ResolutionError(structName, context.scope, getLocation());
	if (auto struct_type = type->ptrcast<StructType>())
		return struct_type;
	throw NotStructError(type, getLocation());
}

std::string StaticFieldExpr::mangle(const Context &context) const {
	const auto &statics = getStruct(context)->getStatics();
	if (statics.count(fieldName) == 0)
		throw ResolutionError(fieldName, Context(*context.program, context.scope, structName), getLocation());
	return Util::mangleStaticField(structName, statics.at(fieldName), fieldName);
}
