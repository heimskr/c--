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
	GenericError(const ASTLocation &location_, Args &&...args):
		std::runtime_error(std::forward<Args>(args)...), location(location_) {}
};

struct ResolutionError: GenericError {
	std::shared_ptr<Scope> scope;
	std::string name;
	std::string structName;
	ResolutionError(const std::string &name_, std::shared_ptr<Scope> scope_, const ASTLocation &location_ = {}):
		GenericError(location_, "Couldn't resolve symbol " + name_), scope(scope_), name(name_) {}
	ResolutionError(const std::string &name_, const Context &, const ASTLocation &location_ = {});
};

struct LvalueError: GenericError {
	std::string typeString;
	LvalueError(const std::string &type_string, const ASTLocation &location_ = {}):
		GenericError(location_, "Not an lvalue: " + type_string), typeString(type_string) {}
};

struct FunctionPointerError: GenericError {
	std::string typeString;
	FunctionPointerError(const std::string &type_string, const ASTLocation &location_ = {}):
		GenericError(location_, "Not a function pointer: " + type_string), typeString(type_string) {}
};

struct NotOnStackError: GenericError {
	std::shared_ptr<Variable> variable;
	NotOnStackError(std::shared_ptr<Variable>, const ASTLocation &location_ = {});
};

struct ImplicitConversionError: GenericError {
	std::shared_ptr<Type> left, right;
	ImplicitConversionError(std::shared_ptr<Type> left_, std::shared_ptr<Type> right_, const ASTLocation &location_);
	ImplicitConversionError(std::shared_ptr<Type> left_, std::shared_ptr<Type> right_);
	ImplicitConversionError(const Type &left_, const Type &right_, const ASTLocation &location_);
	ImplicitConversionError(const Type &left_, const Type &right_);
};

struct NotPointerError: GenericError {
	std::shared_ptr<Type> type;
	NotPointerError(std::shared_ptr<Type> type_, const ASTLocation &location_ = {});
};

struct NotArrayError: GenericError {
	std::shared_ptr<Type> type;
	NotArrayError(std::shared_ptr<Type> type_, const ASTLocation &location_ = {});
};

struct NotStructError: GenericError {
	std::shared_ptr<Type> type;
	NotStructError(std::shared_ptr<Type> type_, const ASTLocation &location_ = {});
};

struct NameConflictError: GenericError {
	std::string name;
	NameConflictError(const std::string &name_, const ASTLocation &location_ = {}):
		GenericError(location_, "Name collision encountered: " + name_), name(name_) {}
};

struct UncolorableError: std::runtime_error {
	UncolorableError(): std::runtime_error("Unable to color graph: not enough colors") {}
};

struct IncompleteStructError: std::runtime_error {
	std::string name;
	IncompleteStructError(const std::string &name_):
		std::runtime_error("Can't access incomplete struct " + name_), name(name_) {}
};

struct ConstError: GenericError {
	std::string typeString;
	ConstError(const std::string &start, const std::string &type_string, const ASTLocation &location_ = {}):
		GenericError(location_, (start.empty()? "" : start + ": ") + type_string + " is const") {}
};

struct InvalidFunctionNameError: GenericError {
	std::string name;
	InvalidFunctionNameError(const std::string &name_, const ASTLocation &location_ = {}):
		GenericError(location_, "Invalid function name: " + name_), name(name_) {}
};
