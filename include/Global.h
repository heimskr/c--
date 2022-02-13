#pragma once

#include <memory>
#include <string>

class ASTNode;
struct Expr;
struct Type;

struct Global {
	std::string name;
	std::shared_ptr<Type> type;
	std::shared_ptr<Expr> value;

	Global(const std::string &name_, std::shared_ptr<Type> type_, std::shared_ptr<Expr> value_):
		name(name_), type(type_), value(value_) {}
};
