#include <sstream>

#include "ASTNode.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
#include "Scope.h"
#include "Type.h"

Type::operator std::string() const {
	if (isConst)
		return stringify() + " const";
	return stringify();
}

std::ostream & operator<<(std::ostream &os, const Type &type) {
	return os << std::string(type);
}

bool SignedType::operator&&(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (other.isBool())
		return true;
	if (auto *other_signed = other.cast<SignedType>())
		return other_signed->width == width;
	return false;
}

bool SignedType::operator==(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (auto *other_signed = other.cast<SignedType>())
		return other_signed->width == width;
	return false;
}

bool UnsignedType::operator&&(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (other.cast<BoolType>())
		return true;
	if (auto *other_unsigned = other.cast<UnsignedType>())
		return other_unsigned->width == width;
	return false;
}

bool UnsignedType::operator==(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (auto *other_unsigned = other.cast<UnsignedType>())
		return other_unsigned->width == width;
	return false;
}

bool PointerType::operator&&(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (other.isBool())
		return true;
	if (auto *other_pointer = other.cast<PointerType>()) {
		if (subtype->isVoid() || other_pointer->subtype->isVoid() || (*subtype && *other_pointer->subtype))
			return true;
		if (auto *subtype_array = subtype->cast<ArrayType>())
			return *subtype_array->subtype && *other_pointer->subtype;
	}
	return false;
}

bool PointerType::operator==(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (auto *other_ptr = other.cast<PointerType>())
		return *other_ptr->subtype == *subtype;
	return false;
}

int PointerType::affinity(const Type &other) const {
	if (other.isBool())
		return 1;
	if (auto *other_pointer = other.cast<PointerType>()) {
		if (subtype->isConst && !other_pointer->subtype->isConst)
			return 0;
		if (subtype->isVoid())
			return other_pointer->subtype->isVoid()? 3 : 2;
		if (other_pointer->subtype->isVoid())
			return 2;
		if (*subtype == *other_pointer->subtype)
			return subtype->affinity(*other_pointer->subtype) + 1;
		if (auto *subtype_array = subtype->cast<ArrayType>())
			return subtype->affinity(*subtype_array->subtype) + 1;
	}
	return 0;
}

bool ReferenceType::operator&&(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (auto *other_reference = other.cast<ReferenceType>()) {
		if (subtype->isVoid() || other_reference->subtype->isVoid() || (*subtype && *other_reference->subtype))
			return true;
		if (auto *subtype_array = subtype->cast<ArrayType>())
			return *subtype_array->subtype && *other_reference->subtype;
	}
	return false;
}

bool ReferenceType::operator==(const Type &other) const {
	if (auto *other_ptr = other.cast<ReferenceType>())
		return *other_ptr->subtype == *subtype;
	return false;
}

int ReferenceType::affinity(const Type &other) const {
	if (auto *other_reference = other.cast<ReferenceType>()) {
		if (subtype->isConst && !other_reference->subtype->isConst)
			return 0;
		if (subtype->isVoid())
			return other_reference->subtype->isVoid()? 3 : 2;
		if (other_reference->subtype->isVoid())
			return 2;
		if (*subtype == *other_reference->subtype)
			return subtype->affinity(*other_reference->subtype) + 1;
		if (auto *subtype_array = subtype->cast<ArrayType>())
			return subtype->affinity(*subtype_array->subtype) + 1;
	}
	return 0;
}

bool ReferenceType::isReferenceOf(const Type &other) const {
	return *subtype == other;
}

bool ArrayType::operator&&(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (auto *other_array = other.cast<ArrayType>()) {
		if (subtype->isConst && !other_array->subtype->isConst)
			return false;
		return (*subtype && *other_array->subtype) && count == other_array->count;
	}
	if (auto *pointer = other.cast<PointerType>()) {
		if (subtype->isConst && !pointer->subtype->isConst)
			return false;
		return *subtype && *pointer->subtype;
	}
	return false;
}

bool ArrayType::operator==(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (auto *other_array = other.cast<ArrayType>())
		return (*subtype == *other_array->subtype) && count == other_array->count;
	return false;
}

Type * Type::get(const ASTNode &node, Program &program, bool allow_forward) {
	switch (node.symbol) {
		case CPMTOK_VOID:
			return new VoidType;
		case CPMTOK_BOOL:
			return new BoolType;
		case CPMTOK_S8:
			return new SignedType(8);
		case CPMTOK_S16:
			return new SignedType(16);
		case CPMTOK_S32:
			return new SignedType(32);
		case CPMTOK_S64:
			return new SignedType(64);
		case CPMTOK_U8:
			return new UnsignedType(8);
		case CPMTOK_U16:
			return new UnsignedType(16);
		case CPMTOK_U32:
			return new UnsignedType(32);
		case CPMTOK_U64:
			return new UnsignedType(64);
		case CPMTOK_TIMES: {
			Type *subtype = Type::get(*node.front(), program, true);
			if (subtype->isReference())
				throw GenericError(node.location, "Cannot take a pointer to a reference");
			return new PointerType(subtype);
		}
		case CPMTOK_AND: {
			Type *subtype = Type::get(*node.front(), program, true);
			if (subtype->isReference())
				throw GenericError(node.location, "Cannot take a reference to a reference");
			return new ReferenceType(subtype);
		}
		case CPMTOK_STRING:
			return new PointerType(new UnsignedType(8));
		case CPMTOK_LSQUARE: {
			auto expr = std::unique_ptr<Expr>(Expr::get(*node.at(1)));
			auto count = expr->evaluate({program, nullptr});
			if (!count)
				throw GenericError(node.location, "Array size expression must be a compile-time constant: " +
					std::string(*expr) + " (at " + std::string(expr->getLocation()) + ")");
			return new ArrayType(Type::get(*node.front(), program), *count);
		}
		case CPM_FNPTR: {
			std::vector<Type *> argument_types;
			argument_types.reserve(node.at(1)->size());
			for (const ASTNode *child: *node.at(1))
				argument_types.push_back(Type::get(*child, program));
			return new FunctionPointerType(Type::get(*node.front(), program), std::move(argument_types));
		}
		case CPMTOK_MOD: {
			const std::string &struct_name = *node.front()->text;
			if (program.structs.count(struct_name) != 0)
				return program.structs.at(struct_name)->copy();
			if (program.forwardDeclarations.count(struct_name) != 0) {
				if (allow_forward)
					return new StructType(program, struct_name);
				throw GenericError(node.location, "Can't use forward declaration of " + struct_name +
					" in this context");
			}
			throw ResolutionError(struct_name, nullptr);
		}
		case CPMTOK_CONST: {
			Type *subtype = Type::get(*node.front(), program, allow_forward);
			subtype->isConst = true;
			return subtype;
		}
		default:
			throw GenericError(node.location, "Invalid token in getType: " +
				std::string(cpmParser.getName(node.symbol)));
	}
}

Type * Type::get(const std::string &mangled, Program &program) {
	const char *c_str = mangled.c_str();
	return get(c_str, program);
}

Type * Type::get(const char * &mangled, Program &program) {
	switch (mangled[0]) {
		case 'v':
			++mangled;
			return new VoidType;
		case 'b':
			++mangled;
			return new BoolType;
		case 's':
		case 'u': {
			const char start = mangled[0];
			size_t width = 8 * (*++mangled - '0');
			++mangled;
			if (start == 's')
				return new SignedType(width);
			return new UnsignedType(width);
		}
		case 'p':
			return new PointerType(get(++mangled, program));
		case 'a': {
			size_t count = 0;
			for (++mangled; std::isdigit(mangled[0]); ++mangled)
				count = count * 10 + (mangled[0] - '0');
			return new ArrayType(Type::get(mangled, program), count);
		}
		case 'F': {
			Type *return_type = Type::get(++mangled, program);
			std::vector<Type *> argument_types;
			size_t argument_count = 0;
			for (; std::isdigit(mangled[0]); ++mangled)
				argument_count = argument_count * 10 + (mangled[0] - '0');
			for (size_t i = 0; i < argument_count; ++i)
				argument_types.push_back(Type::get(mangled, program));
			return new FunctionPointerType(return_type, std::move(argument_types));
		}
		case 'S': {
			size_t width = 0;
			for (++mangled; std::isdigit(mangled[0]); ++mangled)
				width = width * 10 + (mangled[0] - '0');
			const std::string struct_name(mangled, width);
			mangled += width;
			if (program.structs.count(struct_name) != 0)
				return program.structs.at(struct_name)->copy();
			if (program.forwardDeclarations.count(struct_name) != 0)
				return new StructType(program, struct_name);
			throw std::runtime_error("Couldn't find struct " + struct_name);
		}
		default:
			throw std::runtime_error("Cannot demangle \"" + std::string(mangled) + "\"");
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
	return (new FunctionPointerType(returnType->copy(), std::move(arguments_copy)))->steal(*this);
}

std::string FunctionPointerType::mangle() const {
	std::stringstream out;
	out << 'F' << returnType->mangle() << argumentTypes.size();
	for (const Type *argument_type: argumentTypes)
		out << argument_type->mangle();
	return out.str();
}

std::string FunctionPointerType::stringify() const {
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
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (auto *ptr = other.cast<PointerType>())
		return ptr->subtype->isVoid();
	return *this == other;
}

bool FunctionPointerType::operator==(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
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
		argumentTypes.push_back(function.argumentMap.at(name)->getType()->copy());
}

StructType::StructType(const Program &program_, const std::string &name_):
	program(program_), name(name_), isForwardDeclaration(true) {}

StructType::StructType(const Program &program_, const std::string &name_, const decltype(order) &order_,
                       const decltype(statics) &statics_):
order(order_), statics(statics_), program(program_), name(name_) {
	for (const auto &pair: order_)
		map.insert(pair);
}

StructType::StructType(const Program &program_, const std::string &name_, decltype(order) &&order_,
                       decltype(statics) &&statics_):
order(std::move(order_)), statics(std::move(statics_)), program(program_), name(name_) {
	for (const auto &pair: order)
		map.insert(pair);
}

Type * StructType::copy() const {
	if (isForwardDeclaration)
		return new StructType(program, name);
	auto *out = new StructType(program, name, getOrder(), getStatics());
	out->steal(*this);
	out->destructor = destructor;
	return out;
}

std::string StructType::stringify() const {
	return "%" + name;
}

size_t StructType::getSize() const {
	size_t out = 0;
	for (const auto &[field_name, field_type]: getOrder())
		out += field_type->getSize();
	return out;
}

bool StructType::operator&&(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	return *this == other;
}

bool StructType::operator==(const Type &other) const {
	if (other.isReferenceOf(*this))
		return isLvalue;
	if (const auto *other_struct = other.cast<StructType>())
		return name == other_struct->name && !(isConst && !other_struct->isConst);
	return false;
}

size_t StructType::getFieldOffset(const std::string &field_name) const {
	size_t offset = 0;
	for (const auto &[order_name, order_type]: getOrder()) {
		if (order_name == field_name)
			return offset;
		offset += order_type->getSize();
	}
	throw ResolutionError(field_name, nullptr);
}

size_t StructType::getFieldSize(const std::string &field_name) const {
	if (getMap().count(field_name) == 0)
		throw ResolutionError(field_name, nullptr);
	return getMap().at(field_name)->getSize();
}

const decltype(StructType::order) & StructType::getOrder() const {
	if (isForwardDeclaration) {
		if (program.structs.count(name) == 0)
			throw IncompleteStructError(name);
		return program.structs.at(name)->order;
	}

	return order;
}

const decltype(StructType::map) & StructType::getMap() const {
	if (isForwardDeclaration) {
		if (program.structs.count(name) == 0)
			throw IncompleteStructError(name);
		return program.structs.at(name)->map;
	}

	return map;
}

const decltype(StructType::statics) & StructType::getStatics() const {
	if (isForwardDeclaration) {
		if (program.structs.count(name) == 0)
			throw IncompleteStructError(name);
		return program.structs.at(name)->statics;
	}

	return statics;
}

FunctionPtr StructType::getDestructor() const {
	if (isForwardDeclaration) {
		if (program.structs.count(name) == 0)
			throw IncompleteStructError(name);
		return program.structs.at(name)->destructor.lock();
	}

	return destructor.lock();
}

InitializerType::InitializerType(const std::vector<ExprPtr> &exprs, const Context &context) {
	for (const auto &expr: exprs)
		children.emplace_back(expr->getType(context));
}

Type * InitializerType::copy() const {
	std::vector<TypePtr> children_copy;
	for (const auto &child: children)
		children_copy.emplace_back(child->copy());
	return (new InitializerType(std::move(children_copy)))->steal(*this);
}

bool InitializerType::operator&&(const Type &other) const {
	if (const auto *other_init = other.cast<InitializerType>()) {
		const size_t max = children.size();
		if (other_init->children.size() != max)
			return false;
		for (size_t i = 0; i < max; ++i)
			if (!(*children[i] && *other_init->children[i]))
				return false;
		return true;
	} else if (const auto *other_struct = other.cast<StructType>()) {
		const auto &fields = other_struct->getOrder();
		const size_t max = children.size();
		if (fields.size() != max)
			return false;
		for (size_t i = 0; i < max; ++i)
			if (*children[i] != *fields[i].second)
				return false;
		return true;
	} else if (const auto *other_array = other.cast<ArrayType>()) {
		if (other_array->count != children.size())
			return false;
		for (const auto &child: children)
			if (*child != *other_array->subtype)
				return false;
		return true;
	}

	return false;
}

bool InitializerType::operator==(const Type &other) const {
	if (const auto *other_init = other.cast<InitializerType>()) {
		const size_t max = children.size();
		if (other_init->children.size() != max)
			return false;
		for (size_t i = 0; i < max; ++i)
			if (*children[i] != *other_init->children[i])
				return false;
		return true;
	}

	return false;
}

size_t InitializerType::getSize() const {
	throw std::runtime_error("Can't get size of initializer");
}

std::string InitializerType::stringify() const {
	std::stringstream out;
	out << '[';
	bool first = true;
	for (const auto &child: children) {
		if (first)
			first = false;
		else
			out << ", ";
		out << *child;
	}
	out << ']';
	return out.str();
}
