#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "ASTNode.h"

class Function;
struct Scope;
struct Type;
struct Variable;

struct LocatedError: std::runtime_error {
	ASTLocation location;
	using std::runtime_error::runtime_error;
	template <typename... Args>
	LocatedError(const ASTLocation &location_, Args &&...args):
		std::runtime_error(std::forward<Args>(args)...), location(location_) {}
};

struct ResolutionError: LocatedError {
	std::shared_ptr<Scope> scope;
	std::string name;
	ResolutionError(const std::string &name_, std::shared_ptr<Scope> scope_, const ASTLocation &location_ = {}):
		LocatedError(location_, "Couldn't resolve symbol " + name_), scope(scope_), name(name_) {}
};

struct LvalueError: LocatedError {
	std::string typeString;
	LvalueError(const std::string &type_string, const ASTLocation &location_ = {}):
		LocatedError(location_, "Not an lvalue: " + type_string), typeString(type_string) {}
};

struct FunctionPointerError: LocatedError {
	std::string typeString;
	FunctionPointerError(const std::string &type_string, const ASTLocation &location_ = {}):
		LocatedError(location_, "Not a function pointer: " + type_string), typeString(type_string) {}
};

struct NotOnStackError: LocatedError {
	std::shared_ptr<Variable> variable;
	NotOnStackError(std::shared_ptr<Variable>, const ASTLocation &location_ = {});
};

struct ImplicitConversionError: LocatedError {
	std::shared_ptr<Type> left, right;
	ImplicitConversionError(std::shared_ptr<Type> left_, std::shared_ptr<Type> right_, const ASTLocation &location_);
	ImplicitConversionError(std::shared_ptr<Type> left_, std::shared_ptr<Type> right_);
	ImplicitConversionError(const Type &left_, const Type &right_, const ASTLocation &location_);
	ImplicitConversionError(const Type &left_, const Type &right_);
};

struct NotPointerError: LocatedError {
	std::shared_ptr<Type> type;
	NotPointerError(std::shared_ptr<Type> type_, const ASTLocation &location_ = {});
};

struct NotArrayError: LocatedError {
	std::shared_ptr<Type> type;
	NotArrayError(std::shared_ptr<Type> type_, const ASTLocation &location_ = {});
};

struct NotStructError: LocatedError {
	std::shared_ptr<Type> type;
	NotStructError(std::shared_ptr<Type> type_, const ASTLocation &location_ = {});
};

struct NameConflictError: LocatedError {
	std::string name;
	NameConflictError(const std::string &name_, const ASTLocation &location_ = {}):
		LocatedError(location_, "Name collision encountered: " + name_), name(name_) {}
};

struct UncolorableError: std::runtime_error {
	UncolorableError(): std::runtime_error("Unable to color graph: not enough colors") {}
};

struct IncompleteStructError: std::runtime_error {
	std::string name;
	IncompleteStructError(const std::string &name_):
		std::runtime_error("Can't access incomplete struct " + name_), name(name_) {}
};
