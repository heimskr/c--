#pragma once

#include <memory>
#include <string>

#include "Variable.h"

class ASTNode;
struct Expr;
struct Type;

struct Global: Variable {
	std::shared_ptr<Expr> value;

	Global(const std::string &name_, std::shared_ptr<Type> type_, std::shared_ptr<Expr> value_):
		Variable(name_, std::move(type_)), value(std::move(value_)) {}
};

using GlobalPtr = std::shared_ptr<Global>;
