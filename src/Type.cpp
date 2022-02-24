#include <sstream>

#include "ASTNode.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
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

Type * Type::get(const ASTNode &node, const Program &program, bool allow_forward) {
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
			return new PointerType(Type::get(*node.front(), program, true));
		case CMMTOK_STRING:
			return new PointerType(new UnsignedType(8));
		case CMMTOK_LSQUARE: {
			auto expr = std::unique_ptr<Expr>(Expr::get(*node.at(1)));
			auto count = expr->evaluate(nullptr);
			if (!count)
				throw std::runtime_error("Array size expression must be a compile-time constant: " + std::string(*expr)
					+ " (at " + std::string(expr->location) + ")");
			return new ArrayType(Type::get(*node.front(), program), *count);
		}
		case CMM_FNPTR: {
			std::vector<Type *> argument_types;
			argument_types.reserve(node.at(1)->size());
			for (const ASTNode *child: *node.at(1))
				argument_types.push_back(Type::get(*child, program));
			return new FunctionPointerType(Type::get(*node.front(), program), std::move(argument_types));
		}
		case CMMTOK_STRUCT: {
			const std::string &struct_name = *node.front()->text;
			if (program.structs.count(struct_name) != 0)
				return program.structs.at(struct_name)->copy();
			if (program.forwardDeclarations.count(struct_name) != 0) {
				if (allow_forward)
					return new StructType(struct_name);
				throw std::runtime_error("Can't use forward declaration of " + struct_name + " in this context");
			}
			throw ResolutionError(struct_name, nullptr);
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

StructType::StructType(const std::string &name_):
	name(name_), isForwardDeclaration(true) {}

StructType::StructType(const std::string &name_, const decltype(order) &order_): name(name_), order(order_) {
	for (const auto &pair: order_)
		map.insert(pair);
}

Type * StructType::copy() const {
	decltype(order) order_copy;
	for (const auto &[field_name, field_type]: order)
		order_copy.emplace_back(field_name, TypePtr(field_type->copy()));
	return new StructType(name, order_copy);
}

StructType::operator std::string() const {
	std::stringstream out;
	out << "struct " << name << " {";
	for (const auto &[field_name, field_type]: order)
		out << ' ' << field_name << ": " << *field_type << ';';
	if (!order.empty())
		out << ' ';
	out << '}';
	return out.str();
}

size_t StructType::getSize() const {
	size_t out = 0;
	for (const auto &[field_name, field_type]: order)
		out += field_type->getSize();
	return out;
}

bool StructType::operator&&(const Type &other) const {
	return *this == other;
}

bool StructType::operator==(const Type &other) const {
	if (const auto *other_struct = other.cast<StructType>())
		return name == other_struct->name;
	return false;
}

size_t StructType::getFieldOffset(const std::string &name) const {
	size_t offset = 0;
	for (const auto &[field_name, field_type]: order) {
		if (field_name == name)
			return offset;
		offset += field_type->getSize();
	}
	throw ResolutionError(name, nullptr);
}

size_t StructType::getFieldSize(const std::string &name) const {
	if (map.count(name) == 0)
		throw ResolutionError(name, nullptr);
	return map.at(name)->getSize();
}
