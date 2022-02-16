#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "ASTNode.h"

class Function;
struct Scope;
struct Type;
struct Variable;

struct ResolutionError: std::runtime_error {
	std::shared_ptr<Scope> scope;
	std::string name;
	ResolutionError(const std::string &name_, std::shared_ptr<Scope> scope_):
		std::runtime_error("Couldn't resolve symbol " + name_), scope(scope_), name(name_) {}
};

struct LvalueError: std::runtime_error {
	std::string typeString;
	LvalueError(const std::string &type_string):
		std::runtime_error("Not an lvalue: " + type_string), typeString(type_string) {}
};

struct NotOnStackError: std::runtime_error {
	std::shared_ptr<Variable> variable;
	NotOnStackError(std::shared_ptr<Variable>);
};

struct ImplicitConversionError: std::runtime_error {
	std::shared_ptr<Type> left, right;
	ImplicitConversionError(std::shared_ptr<Type> left_, std::shared_ptr<Type> right_);
	ImplicitConversionError(const Type &left_, const Type &right_);
};

struct NotPointerError: std::runtime_error {
	std::shared_ptr<Type> type;
	NotPointerError(std::shared_ptr<Type> type_);
};

struct NameConflictError: std::runtime_error {
	std::string name;
	ASTLocation location;
	NameConflictError(const std::string &name_, const ASTLocation &location_ = {0, 0}):
		std::runtime_error("Name collision encountered" + (location_.line? " at " + std::string(location_): "") + ": " +
			name_), name(name_),
		location(location_) {}
};
