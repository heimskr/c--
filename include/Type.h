#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "Checkable.h"
#include "Makeable.h"

class ASTNode;

struct Type: Checkable, std::enable_shared_from_this<Type> {
	virtual ~Type() {}
	virtual Type * copy() const = 0;
	virtual operator std::string() const = 0;
	virtual bool isNumber(size_t = 0) const { return false; }
	virtual bool isSigned(size_t = 0) const { return false; }
	virtual bool isUnsigned(size_t = 0) const { return false; }
	virtual size_t getSize() const = 0; // in bytes
	/** Returns whether this type can be implicitly converted to the given type. Order matters! */
	virtual bool operator&&(const Type &) const { return false; }
	virtual bool isInt()  const { return false; }
	virtual bool isVoid() const { return false; }
	virtual bool isBool() const { return false; }
	virtual bool isPointer() const { return false; }
	static Type * get(const ASTNode &);

	template <typename T>
	std::shared_ptr<T> ptrcast() {
		return std::dynamic_pointer_cast<T>(shared_from_this());
	}

	template <typename T>
	std::shared_ptr<const T> ptrcast() const {
		return std::dynamic_pointer_cast<const T>(shared_from_this());
	}
};

struct IntType: Type {
	size_t width; // in bits
	bool isNumber(size_t width_) const override { return width_ == 0 || width_ == width; }
	size_t getSize() const override { return width / 8; }
	bool isInt() const override { return true; }

	protected:
		IntType(size_t width_): width(width_) {}
};

struct SignedType: IntType {
	SignedType(size_t width_): IntType(width_) {}
	Type * copy() const override { return new SignedType(width); }
	bool isSigned(size_t width_) const override { return isNumber(width_); }
	operator std::string() const override { return "s" + std::to_string(width); }
	bool operator&&(const Type &) const override;
};

struct UnsignedType: IntType {
	UnsignedType(size_t width_): IntType(width_) {}
	Type * copy() const override { return new UnsignedType(width); }
	bool isUnsigned(size_t width_) const override { return isNumber(width_); }
	operator std::string() const override { return "u" + std::to_string(width); }
	bool operator&&(const Type &) const override;
};

struct VoidType: Type, Makeable<VoidType> {
	Type * copy() const override { return new VoidType; }
	operator std::string() const override { return "void"; }
	size_t getSize() const override { return 0; }
	bool isVoid() const override { return true; }
};

struct BoolType: Type {
	Type * copy() const override { return new BoolType; }
	operator std::string() const override { return "bool"; }
	size_t getSize() const override { return 1; }
	bool operator&&(const Type &) const override;
	bool isBool() const override { return true; }
};

struct PointerType: Type {
	Type *subtype;
	/** Takes ownership of the subtype pointer! */
	PointerType(Type *subtype_): subtype(subtype_) {}
	~PointerType() { if (subtype) delete subtype; }
	Type * copy() const override { return new PointerType(subtype? subtype->copy() : nullptr); }
	operator std::string() const override { return subtype? std::string(*subtype) + "*" : "???*"; }
	size_t getSize() const override { return 8; }
	bool operator&&(const Type &) const override;
	bool isPointer() const override { return true; }
};

using TypePtr = std::shared_ptr<Type>;
