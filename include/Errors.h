#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "ASTNode.h"

class Function;
struct Context;
struct Scope;
struct Type;
struct Variable;

struct GenericError: std::runtime_error {
	ASTLocation location;
	using std::runtime_error::runtime_error;
	template <typename... Args>
	explicit GenericError(const ASTLocation &location_, Args &&...args):
		std::runtime_error(std::forward<Args>(args)...), location(location_) {}
} __attribute__((packed, aligned(16)));

struct ResolutionError: GenericError {
	std::shared_ptr<Scope> scope;
	std::string name;
	std::string structName;
	explicit ResolutionError(const std::string &name_, std::shared_ptr<Scope> scope_, ASTLocation location_ = {}):
		GenericError(location_, "Couldn't resolve symbol " + name_), scope(std::move(scope_)), name(name_) {}
	explicit ResolutionError(const std::string &name_, const Context &, ASTLocation location_ = {});
};

struct LvalueError: GenericError {
	std::string typeString;
	explicit LvalueError(const std::string &type_string, const ASTLocation &location_ = {}):
		GenericError(location_, "Not an lvalue: " + type_string), typeString(type_string) {}
};

struct FunctionPointerError: GenericError {
	std::string typeString;
	explicit FunctionPointerError(const std::string &type_string, ASTLocation location_ = {}):
		GenericError(location_, "Not a function pointer: " + type_string), typeString(type_string) {}
};

struct NotOnStackError: GenericError {
	std::shared_ptr<Variable> variable;
	explicit NotOnStackError(const std::shared_ptr<Variable> &, ASTLocation location_ = {});
};

struct ImplicitConversionError: GenericError {
	std::shared_ptr<Type> left, right;
	explicit ImplicitConversionError(const std::shared_ptr<Type> &left_, const std::shared_ptr<Type> &right_,
	                                 ASTLocation location_ = {});
	explicit ImplicitConversionError(const Type &left_, const Type &right_, ASTLocation location_);
	explicit ImplicitConversionError(const Type &left_, const Type &right_);
};

struct NotPointerError: GenericError {
	std::shared_ptr<Type> type;
	explicit NotPointerError(const std::shared_ptr<Type> &type_, ASTLocation location_ = {});
};

struct NotArrayError: GenericError {
	std::shared_ptr<Type> type;
	explicit NotArrayError(const std::shared_ptr<Type> &type_, ASTLocation location_ = {});
};

struct NotStructError: GenericError {
	std::shared_ptr<Type> type;
	explicit NotStructError(const std::shared_ptr<Type> &type_, ASTLocation location_ = {});
};

struct NameConflictError: GenericError {
	std::string name;
	explicit NameConflictError(const std::string &name_, ASTLocation location_ = {}):
		GenericError(location_, "Name collision encountered: " + name_), name(name_) {}
};

struct UncolorableError: std::runtime_error {
	UncolorableError(): std::runtime_error("Unable to color graph: not enough colors") {}
};

struct IncompleteStructError: std::runtime_error {
	std::string name;
	explicit IncompleteStructError(const std::string &name_):
		std::runtime_error("Can't access incomplete struct " + name_), name(name_) {}
};

struct ConstError: GenericError {
	std::string typeString;
	ConstError(const std::string &start, const std::string &type_string, ASTLocation location_ = {}):
		GenericError(location_, (start.empty()? "" : start + ": ") + type_string + " is const") {}
};

struct InvalidFunctionNameError: GenericError {
	std::string name;
	explicit InvalidFunctionNameError(const std::string &name_, ASTLocation location_ = {}):
		GenericError(location_, "Invalid function name: " + name_), name(name_) {}
};

struct AmbiguousError: GenericError {
	using GenericError::GenericError;
};
