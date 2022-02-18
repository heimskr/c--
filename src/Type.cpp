#include "ASTNode.h"
#include "Lexer.h"
#include "Parser.h"
#include "Type.h"

std::ostream & operator<<(std::ostream &os, const Type &type) {
	return os << std::string(type);
}

bool SignedType::operator&&(const Type &other) const {
	if (auto *other_signed = dynamic_cast<const SignedType *>(&other))
		return other_signed->width == width;
	return false;
}

bool UnsignedType::operator&&(const Type &other) const {
	if (auto *other_unsigned = dynamic_cast<const UnsignedType *>(&other))
		return other_unsigned->width == width;
	return false;
}

bool BoolType::operator&&(const Type &other) const {
	return dynamic_cast<const BoolType *>(&other);
}

bool PointerType::operator&&(const Type &other) const {
	if (auto *other_pointer = dynamic_cast<const PointerType *>(&other))
		return dynamic_cast<const VoidType *>(other_pointer->subtype) || (*subtype && *other_pointer->subtype);
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
		default:
			throw std::invalid_argument("Invalid token in getType: " + std::string(cmmParser.getName(node.symbol)));
	}
}
