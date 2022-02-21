#include "ASTNode.h"
#include "Expr.h"
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

bool UnsignedType::operator&&(const Type &other) const {
	if (other.cast<BoolType>())
		return true;
	if (auto *other_unsigned = other.cast<UnsignedType>())
		return other_unsigned->width == width;
	return false;
}

bool BoolType::operator&&(const Type &other) const {
	return other.cast<BoolType>();
}

bool PointerType::operator&&(const Type &other) const {
	if (auto *other_pointer = dynamic_cast<const PointerType *>(&other)) {
		if (other_pointer->subtype->cast<VoidType>() || (*subtype && *other_pointer->subtype))
			return true;
		if (auto *subtype_array = subtype->cast<ArrayType>())
			return *subtype_array->subtype && *other_pointer->subtype;
	}
	return false;
}

bool ArrayType::operator&&(const Type &other) const {
	if (auto *other_array = dynamic_cast<const ArrayType *>(&other))
		return (*subtype && *other_array->subtype) && count == other_array->count;
	if (auto *pointer = dynamic_cast<const PointerType *>(&other))
		return *subtype && *pointer->subtype;
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
		default:
			throw std::invalid_argument("Invalid token in getType: " + std::string(cmmParser.getName(node.symbol)));
	}
}
