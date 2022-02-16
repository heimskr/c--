#pragma once

#include <memory>
#include <string>

class Function;
struct Type;

struct VirtualRegister {
	int id;
	int reg = -1;

	VirtualRegister(int id_): id(id_) {}
	VirtualRegister(Function &function);

	std::string regOrID() const;
};

struct Variable: std::enable_shared_from_this<Variable> {
	Function *function;
	std::string name;
	std::shared_ptr<Type> type;

	Variable(const std::string &name_, std::shared_ptr<Type> type_, Function &function_):
		function(&function_), name(name_), type(type_) {}

	size_t getSize() const {
		return type->getSize();
	}
};

using VregPtr = std::shared_ptr<VirtualRegister>;
using VariablePtr = std::shared_ptr<Variable>;
