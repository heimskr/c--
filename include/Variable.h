#pragma once

#include <memory>
#include <string>

#include "Function.h"
#include "Why.h"

struct Variable {
	int id;
	int reg = -1;

	Variable(int id_): id(id_) {}
	Variable(Function &function): id(function.nextVariable++) {}

	std::string regOrID() const { return reg == -1? std::to_string(id) : Why::registerName(reg); }
};

using VariablePtr = std::shared_ptr<Variable>;
