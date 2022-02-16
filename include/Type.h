#pragma once

#include <cstddef>
#include <string>

class ASTNode;

struct Type {
	virtual Type * copy() const = 0;
	virtual operator std::string() const = 0;
	virtual bool isNumber() const { return false; }
	virtual size_t getSize() const = 0; // in bytes
	virtual ~Type() {}
};

struct IntType: Type {
	size_t width;
	bool isNumber() const override { return true; }
	size_t getSize() const override { return width / 8; }

	protected:
		IntType(size_t width_): width(width_) {}
};

struct SignedType: IntType {
	SignedType(size_t width_): IntType(width_) {}
	Type * copy() const override { return new SignedType(width); }
	operator std::string() const override { return "s" + std::to_string(width); }
};

struct UnsignedType: IntType {
	UnsignedType(size_t width_): IntType(width_) {}
	Type * copy() const override { return new UnsignedType(width); }
	operator std::string() const override { return "u" + std::to_string(width); }
};

struct VoidType: Type {
	Type * copy() const override { return new VoidType; }
	operator std::string() const override { return "void"; }
	size_t getSize() const override { return 0; }
};

struct BoolType: Type {
	Type * copy() const override { return new BoolType; }
	operator std::string() const override { return "bool"; }
	size_t getSize() const override { return 1; }
};

struct PointerType: Type {
	Type *subtype;
	PointerType(Type *subtype_): subtype(subtype_) {}
	~PointerType() { if (subtype) delete subtype; }
	Type * copy() const override { return new PointerType(subtype? subtype->copy() : nullptr); }
	operator std::string() const override { return subtype? std::string(*subtype) + "*" : "???*"; }
	size_t getSize() const override { return 8; }
};

Type * getType(const ASTNode &);
