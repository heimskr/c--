#include <sstream>

#include "ASTNode.h"
#include "Expr.h"
#include "Function.h"
#include "Lexer.h"
#include "Parser.h"
#include "Type.h"

std::ostream & operator<<(std::ostream &os, const Type &type) {
	return os << std::string(type);
}

bool SignedType::operator&&(const Type &other) const {
	if (other.cast<BoolType>())
		return true;
	if (auto *other_signed = other.cast<SignedType>())
		return other_signed->width == width;
	return false;
}

bool SignedType::operator==(const Type &other) const {
	if (auto *other_signed = other.cast<SignedType>())
		return other_signed->width == width;
	return false;
}

bool UnsignedType::operator&&(const Type &other) const {
	if (other.cast<BoolType>())
		return true;
	if (auto *other_unsigned = other.cast<UnsignedType>())
		return other_unsigned->width == width;
	return false;
}

bool UnsignedType::operator==(const Type &other) const {
	if (auto *other_unsigned = other.cast<UnsignedType>())
		return other_unsigned->width == width;
	return false;
}

bool PointerType::operator&&(const Type &other) const {
	if (auto *other_pointer = other.cast<PointerType>()) {
		if (other_pointer->subtype->cast<VoidType>() || (*subtype && *other_pointer->subtype))
			return true;
		if (auto *subtype_array = subtype->cast<ArrayType>())
			return *subtype_array->subtype && *other_pointer->subtype;
	}
	return false;
}

bool PointerType::operator==(const Type &other) const {
	if (auto *other_ptr = other.cast<PointerType>())
		return *other_ptr->subtype == *subtype;
	return false;
}

bool ArrayType::operator&&(const Type &other) const {
	if (auto *other_array = other.cast<ArrayType>())
		return (*subtype && *other_array->subtype) && count == other_array->count;
	if (auto *pointer = other.cast<PointerType>())
		return *subtype && *pointer->subtype;
	return false;
}

bool ArrayType::operator==(const Type &other) const {
	if (auto *other_array = other.cast<ArrayType>())
		return (*subtype == *other_array->subtype) && count == other_array->count;
	return false;
}

Type * Type::get(const ASTNode &node) {
	switch (node.symbol) {
		case CMMTOK_VOID:
			return new VoidType;
		case CMMTOK_BOOL:
			return new BoolType;
		case CMMTOK_S8:
			return new SignedType(8);
		case CMMTOK_S16:
			return new SignedType(16);
		case CMMTOK_S32:
			return new SignedType(32);
		case CMMTOK_S64:
			return new SignedType(64);
		case CMMTOK_U8:
			return new UnsignedType(8);
		case CMMTOK_U16:
			return new UnsignedType(16);
		case CMMTOK_U32:
			return new UnsignedType(32);
		case CMMTOK_U64:
			return new UnsignedType(64);
		case CMMTOK_TIMES:
			return new PointerType(Type::get(*node.front()));
		case CMMTOK_STRING:
			return new PointerType(new UnsignedType(8));
		case CMMTOK_LSQUARE: {
			auto expr = std::unique_ptr<Expr>(Expr::get(*node.at(1)));
			auto count = expr->evaluate(nullptr);
			if (!count)
				throw std::runtime_error("Array size expression must be a compile-time constant: " + std::string(*expr)
					+ " (at " + std::string(expr->location) + ")");
			return new ArrayType(Type::get(*node.front()), *count);
		}
		case CMM_FNPTR: {
			std::vector<Type *> argument_types;
			argument_types.reserve(node.at(1)->size());
			for (const ASTNode *child: *node.at(1))
				argument_types.push_back(Type::get(*child));
			return new FunctionPointerType(Type::get(*node.front()), std::move(argument_types));
		}
		default:
			throw std::invalid_argument("Invalid token in getType: " + std::string(cmmParser.getName(node.symbol)));
	}
}

FunctionPointerType::FunctionPointerType(Type *return_type, std::vector<Type *> &&argument_types):
	returnType(return_type), argumentTypes(std::move(argument_types)) {}

FunctionPointerType::~FunctionPointerType() {
	delete returnType;
	for (Type *type: argumentTypes)
		delete type;
}

Type * FunctionPointerType::copy() const {
	std::vector<Type *> arguments_copy;
	arguments_copy.reserve(argumentTypes.size());
	for (const Type *type: argumentTypes)
		arguments_copy.push_back(type->copy());
	return new FunctionPointerType(returnType->copy(), std::move(arguments_copy));
}

FunctionPointerType::operator std::string() const {
	std::stringstream out;
	out << *returnType << "(";
	bool first = true;
	for (const Type *type: argumentTypes) {
		if (first)
			first = false;
		else
			out << ", ";
		out << *type;
	}
	out << ")*";
	return out.str();
}

bool FunctionPointerType::operator&&(const Type &other) const {
	if (auto *ptr = other.cast<PointerType>())
		return ptr->subtype->isVoid();
	return *this == other;
}

bool FunctionPointerType::operator==(const Type &other) const {
	if (auto *other_fnptr = other.cast<FunctionPointerType>()) {
		if (*returnType != *other_fnptr->returnType)
			return false;
		if (argumentTypes.size() != other_fnptr->argumentTypes.size())
			return false;
		for (size_t i = 0, max = argumentTypes.size(); i < max; ++i)
			if (*argumentTypes[i] != *other_fnptr->argumentTypes[i])
				return false;
		return true;
	}
	return false;
}

FunctionPointerType::FunctionPointerType(const Function &function): returnType(function.returnType->copy()) {
	argumentTypes.reserve(function.arguments.size());
	for (const std::string &name: function.arguments)
		argumentTypes.push_back(function.argumentMap.at(name)->type->copy());
}
