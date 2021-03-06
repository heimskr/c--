#pragma once

#include <memory>
#include <string>

#include "Variable.h"

class ASTNode;
struct Expr;
struct Type;

struct Global: Variable {
	std::shared_ptr<Expr> value;

	Global(const std::string &name_, const std::shared_ptr<Type> &type_, const std::shared_ptr<Expr> &value_):
		Variable(name_, type_), value(value_) {}
};

using GlobalPtr = std::shared_ptr<Global>;
