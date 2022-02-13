#pragma once

#include <memory>
#include <string>

class ASTNode;
struct Type;

struct Global {
	std::string name;
	std::shared_ptr<Type> type;
	const ASTNode *value = nullptr;

	Global(const std::string &name_, std::shared_ptr<Type> type_, const ASTNode *value_):
		name(name_), type(type_), value(value_) {}
};
