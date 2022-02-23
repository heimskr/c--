#pragma once

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>

#include "Checkable.h"
#include "Makeable.h"

class ASTNode;
class Function;
struct Program;

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
	/** Returns whether this type is identical to the given type. Order shouldn't matter. */
	virtual bool operator==(const Type &) const { return false; }
	virtual bool operator!=(const Type &other) const { return !(*this == other); }
	virtual bool isInt()  const { return false; }
	virtual bool isVoid() const { return false; }
	virtual bool isBool() const { return false; }
	virtual bool isPointer() const { return false; }
	virtual bool isArray() const { return false; }
	virtual bool isFunctionPointer() const { return false; }
	virtual bool isStruct() const { return false; }

	static Type * get(const ASTNode &, const Program &, bool allow_forward = false);

	template <typename T>
	std::shared_ptr<T> ptrcast() {
		return std::dynamic_pointer_cast<T>(shared_from_this());
	}

	template <typename T>
	std::shared_ptr<const T> ptrcast() const {
		return std::dynamic_pointer_cast<const T>(shared_from_this());
	}
};

using TypePtr = std::shared_ptr<Type>;

std::ostream & operator<<(std::ostream &, const Type &);

struct IntType: Type {
	size_t width; // in bits
	bool isNumber(size_t width_) const override { return width_ == 0 || width_ == width; }
	size_t getSize() const override { return width / 8; }
	bool isInt() const override { return true; }

	protected:
		IntType(size_t width_): width(width_) {}
};

struct SignedType: IntType, Makeable<SignedType> {
	SignedType(size_t width_): IntType(width_) {}
	Type * copy() const override { return new SignedType(width); }
	bool isSigned(size_t width_) const override { return isNumber(width_); }
	operator std::string() const override { return "s" + std::to_string(width); }
	bool operator&&(const Type &) const override;
	bool operator==(const Type &) const override;
};

struct UnsignedType: IntType, Makeable<UnsignedType> {
	UnsignedType(size_t width_): IntType(width_) {}
	Type * copy() const override { return new UnsignedType(width); }
	bool isUnsigned(size_t width_) const override { return isNumber(width_); }
	operator std::string() const override { return "u" + std::to_string(width); }
	bool operator&&(const Type &) const override;
	bool operator==(const Type &) const override;
};

struct VoidType: Type, Makeable<VoidType> {
	Type * copy() const override { return new VoidType; }
	operator std::string() const override { return "void"; }
	/** Returns 1 for the sake of void pointer arithmetic acting like byte pointer arithmetic. */
	size_t getSize() const override { return 1; }
	bool operator&&(const Type &other) const override { return other.isVoid(); }
	bool operator==(const Type &other) const override { return other.isVoid(); }
	bool isVoid() const override { return true; }
};

struct BoolType: Type, Makeable<BoolType> {
	Type * copy() const override { return new BoolType; }
	operator std::string() const override { return "bool"; }
	size_t getSize() const override { return 1; }
	bool operator&&(const Type &other) const override { return other.isBool(); }
	bool operator==(const Type &other) const override { return other.isBool(); }
	bool isBool() const override { return true; }
};

struct SuperType: Type {
	Type *subtype;
	/** Takes ownership of the subtype pointer! */
	SuperType(Type *subtype_): subtype(subtype_) {}
	~SuperType() { if (subtype) delete subtype; }
};

struct PointerType: SuperType {
	using SuperType::SuperType;
	Type * copy() const override { return new PointerType(subtype? subtype->copy() : nullptr); }
	operator std::string() const override { return subtype? std::string(*subtype) + "*" : "???*"; }
	size_t getSize() const override { return 8; }
	bool operator&&(const Type &) const override;
	bool operator==(const Type &) const override;
	bool isPointer() const override { return true; }
};

struct ArrayType: SuperType {
	size_t count;
	ArrayType(Type *subtype_, size_t count_): SuperType(subtype_), count(count_) {}
	Type * copy() const override { return new ArrayType(subtype? subtype->copy() : nullptr, count); }
	operator std::string() const override {
		return (subtype? std::string(*subtype) : "???") + "[" + std::to_string(count) + "]";
	}
	size_t getSize() const override { return subtype? subtype->getSize() * count : 0; }
	bool operator&&(const Type &) const override;
	bool operator==(const Type &) const override;
	bool isArray() const override { return true; }
};

/** Owns all its subtypes. */
struct FunctionPointerType: Type {
	Type *returnType;
	std::vector<Type *> argumentTypes;
	FunctionPointerType(Type *return_type, std::vector<Type *> &&argument_types);
	FunctionPointerType(const Function &);
	FunctionPointerType(const FunctionPointerType &) = delete;
	FunctionPointerType & operator=(const FunctionPointerType &) = delete;
	~FunctionPointerType();
	Type * copy() const override;
	operator std::string() const override;
	size_t getSize() const override { return 8; }
	bool operator&&(const Type &) const override;
	bool operator==(const Type &) const override;
	bool isFunctionPointer() const override { return true; }
};

struct StructType: Type, Makeable<StructType> {
	std::string name;
	std::vector<std::pair<std::string, TypePtr>> order;
	std::map<std::string, TypePtr> map;
	bool isForwardDeclaration = false;

	StructType(const std::string &name_);
	StructType(const std::string &name_, const decltype(order) &order_);
	Type * copy() const override;
	operator std::string() const override;
	size_t getSize() const override;
	bool operator&&(const Type &) const override;
	bool operator==(const Type &) const override;
	bool isStruct() const override { return true; }
	size_t getFieldOffset(const std::string &) const;
	size_t getFieldSize(const std::string &) const;
};
