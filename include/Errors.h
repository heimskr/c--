#pragma once

#include <memory>
#include <stdexcept>
#include <string>

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
