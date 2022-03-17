#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "Checkable.h"
#include "Context.h"
#include "Makeable.h"
#include "WeakSet.h"

class ASTNode;
class Function;
struct Expr;
struct Program;
struct Scope;

struct Type: Checkable, std::enable_shared_from_this<Type> {
	bool isConst = false, isLvalue = false;
	virtual ~Type() {}
	virtual Type * copy() const = 0;
	operator std::string() const;
	virtual std::string mangle() const = 0;
	virtual bool isNumber(size_t = 0) const { return false; }
	virtual bool isSigned(size_t = 0) const { return false; }
	virtual bool isUnsigned(size_t = 0) const { return false; }
	virtual size_t getSize() const = 0; // in bytes
	/** Returns whether this type can be implicitly converted to the given type. Order matters! */
	bool operator&&(const Type &other) const { return similar(other, false); }
	virtual bool similar(const Type &, bool ignore_const = false) const = 0;
	/** Returns how favorable a conversion from this type to the given type is. Returns 0 if && would return false. */
	virtual int affinity(const Type &other, bool ignore_const) const { return similar(other, ignore_const)? 2 : 0; }
	/** Returns whether this type is identical to the given type. Order shouldn't matter. */
	bool operator==(const Type &other) const { return equal(other, false); }
	virtual bool equal(const Type &, bool ignore_const = false) const = 0;
	virtual bool operator!=(const Type &other) const { return !(*this == other); }
	virtual bool isInt()  const { return false; }
	virtual bool isVoid() const { return false; }
	virtual bool isBool() const { return false; }
	virtual bool isPointer() const { return false; }
	virtual bool isArray() const { return false; }
	virtual bool isFunctionPointer() const { return false; }
	virtual bool isStruct() const { return false; }
	virtual bool isInitializer() const { return false; }
	virtual bool isReference() const { return false; }
	virtual bool isReferenceOf(const Type &, bool ignore_const) const { (void) ignore_const; return false; }
	Type * setConst(bool is_const) { isConst = is_const; return this; }
	Type * setLvalue(bool is_lvalue) { isLvalue = is_lvalue; return this; }
	Type * steal(const Type &other) { return setConst(other.isConst)->setLvalue(other.isLvalue); }

	static Type * get(const ASTNode &, Program &, bool allow_forward = false);
	static Type * get(const std::string &, Program &);
	static Type * get(const char * &, Program &);

	template <typename T>
	std::shared_ptr<T> ptrcast() {
		return std::dynamic_pointer_cast<T>(shared_from_this());
	}

	template <typename T>
	std::shared_ptr<const T> ptrcast() const {
		return std::dynamic_pointer_cast<const T>(shared_from_this());
	}

	protected:
		virtual std::string stringify() const = 0;
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
	Type * copy() const override { return (new SignedType(width))->steal(*this); }
	bool isSigned(size_t width_) const override { return isNumber(width_); }
	bool similar(const Type &, bool) const override;
	bool equal(const Type &, bool ignore_const) const override;
	std::string mangle() const override { return "s" + std::to_string(width / 8); }
	protected:
		std::string stringify() const override { return "s" + std::to_string(width); }
};

struct UnsignedType: IntType, Makeable<UnsignedType> {
	UnsignedType(size_t width_): IntType(width_) {}
	Type * copy() const override { return (new UnsignedType(width))->steal(*this); }
	bool isUnsigned(size_t width_) const override { return isNumber(width_); }
	bool similar(const Type &, bool) const override;
	bool equal(const Type &, bool ignore_const) const override;
	std::string mangle() const override { return "u" + std::to_string(width / 8); }
	protected:
		std::string stringify() const override { return "u" + std::to_string(width); }
};

struct VoidType: Type, Makeable<VoidType> {
	Type * copy() const override { return (new VoidType)->steal(*this); }
	std::string mangle() const override { return "v"; }
	/** Returns 1 for the sake of void pointer arithmetic acting like byte pointer arithmetic. */
	size_t getSize() const override { return 1; }
	bool similar(const Type &other, bool) const override { return other.isVoid(); }
	bool equal(const Type &other, bool) const override { return other.isVoid(); }
	bool isVoid() const override { return true; }
	protected:
		std::string stringify() const override { return "void"; }
};

struct BoolType: Type, Makeable<BoolType> {
	Type * copy() const override { return (new BoolType)->steal(*this); }
	std::string mangle() const override { return "b"; }
	size_t getSize() const override { return 1; }
	bool similar(const Type &other, bool) const override { return other.isBool(); }
	bool equal(const Type &other, bool) const override { return other.isBool(); }
	bool isBool() const override { return true; }
	protected:
		std::string stringify() const override { return "bool"; }
};

struct SuperType: Type {
	Type *subtype;
	/** Takes ownership of the subtype pointer! */
	SuperType(Type *subtype_): subtype(subtype_) {}
	~SuperType() { if (subtype) delete subtype; }
};

struct PointerType: SuperType, Makeable<PointerType> {
	using SuperType::SuperType;
	Type * copy() const override { return (new PointerType(subtype? subtype->copy() : nullptr))->steal(*this); }
	std::string mangle() const override { return "p" + subtype->mangle(); }
	size_t getSize() const override { return 8; }
	bool similar(const Type &, bool) const override;
	bool equal(const Type &, bool ignore_const) const override;
	bool isPointer() const override { return true; }
	int affinity(const Type &, bool ignore_const) const override;
	protected:
		std::string stringify() const override { return subtype? std::string(*subtype) + "*" : "???*"; }
};

struct ReferenceType: SuperType, Makeable<ReferenceType> {
	ReferenceType(Type *subtype_): SuperType(subtype_) { isLvalue = true; }
	Type * copy() const override { return (new ReferenceType(subtype? subtype->copy() : nullptr))->setConst(isConst); }
	std::string mangle() const override { return "r" + subtype->mangle(); }
	size_t getSize() const override { return subtype->getSize(); }
	bool similar(const Type &, bool) const override;
	bool equal(const Type &, bool ignore_const) const override;
	bool isReference() const override { return true; }
	bool isReferenceOf(const Type &, bool ignore_const) const override;
	int affinity(const Type &, bool ignore_const) const override;
	protected:
		std::string stringify() const override { return subtype? std::string(*subtype) + "&" : "???&"; }
};

struct ArrayType: SuperType {
	size_t count;
	ArrayType(Type *subtype_, size_t count_): SuperType(subtype_), count(count_) {}
	Type * copy() const override {
		return (new ArrayType(subtype? subtype->copy() : nullptr, count))->steal(*this);
	}
	std::string mangle() const override { return "a" + std::to_string(count) + subtype->mangle(); }
	size_t getSize() const override { return subtype? subtype->getSize() * count : 0; }
	bool similar(const Type &, bool) const override;
	bool equal(const Type &, bool ignore_const) const override;
	bool isArray() const override { return true; }
	protected:
		std::string stringify() const override {
			return (subtype? std::string(*subtype) : "???") + "[" + std::to_string(count) + "]";
		}
};

/** Owns all its subtypes. */
struct FunctionPointerType: Type {
	Type *returnType;
	std::vector<Type *> argumentTypes;
	FunctionPointerType(Type *return_type, std::vector<Type *> &&argument_types);
	explicit FunctionPointerType(const Function &);
	FunctionPointerType(const FunctionPointerType &) = delete;
	FunctionPointerType(FunctionPointerType &&) = delete;
	FunctionPointerType & operator=(const FunctionPointerType &) = delete;
	FunctionPointerType & operator=(FunctionPointerType &&) = delete;
	~FunctionPointerType() override;
	Type * copy() const override;
	std::string mangle() const override;
	size_t getSize() const override { return 8; }
	bool similar(const Type &, bool) const override;
	bool equal(const Type &, bool ignore_const) const override;
	bool isFunctionPointer() const override { return true; }
	protected:
		std::string stringify() const override;
};

class StructType: public Type, public Makeable<StructType> {
	private:
		std::map<std::string, TypePtr> map;
		std::vector<std::pair<std::string, TypePtr>> order;
		std::map<std::string, TypePtr> statics;

	public:
		const Program &program;
		std::string name;
		bool isForwardDeclaration = false;
		std::map<std::string, Function *> definedMethods, declaredMethods, definedStaticMethods, declaredStaticMethods;
		std::weak_ptr<Function> destructor;
		WeakSet<Function> constructors;

		StructType(const Program &, const std::string &name_);
		StructType(const Program &, const std::string &name_, const decltype(order) &, const decltype(statics) &);
		StructType(const Program &, const std::string &name_, decltype(order) &&, decltype(statics) &&);
		Type * copy() const override;
		std::string mangle() const override { return "S" + std::to_string(name.size()) + name; }
		size_t getSize() const override;
		bool similar(const Type &, bool) const override;
		bool equal(const Type &, bool ignore_const) const override;
		bool isStruct() const override { return true; }
		size_t getFieldOffset(const std::string &) const;
		size_t getFieldSize(const std::string &) const;
		const decltype(order) & getOrder() const;
		const decltype(map) & getMap() const;
		const decltype(statics) & getStatics() const;
		std::shared_ptr<Function> getDestructor() const;

	protected:
		std::string stringify() const override;
};

struct InitializerType: Type, Makeable<InitializerType> {
	std::vector<TypePtr> children;
	explicit InitializerType(const std::vector<std::shared_ptr<Expr>> &, const Context &);
	explicit InitializerType(const std::vector<TypePtr> &children_): children(children_) {}
	explicit InitializerType(std::vector<TypePtr> &&children_): children(std::move(children_)) {}
	Type * copy() const override;
	std::string mangle() const override { throw std::runtime_error("Cannot mangle InitializerType"); }
	bool isInitializer() const override { return true; }
	bool similar(const Type &, bool) const override;
	bool equal(const Type &, bool ignore_const) const override;
	size_t getSize() const override;
	protected:
		std::string stringify() const override;
};
