#pragma once

#include <memory>
#include <string>

class Function;

struct Variable {
	int id;
	int reg = -1;

	Variable(int id_): id(id_) {}
	Variable(Function &function);

	std::string regOrID() const;
};

using VariablePtr = std::shared_ptr<Variable>;
