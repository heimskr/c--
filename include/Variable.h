#pragma once

#include <memory>
#include <string>

#include "Makeable.h"

class Function;
struct Type;

struct VirtualRegister {
	int id;
	int reg = -1;

	VirtualRegister(int id_): id(id_) {}
	VirtualRegister(Function &function);

	std::string regOrID() const;
};

struct Variable: VirtualRegister, Makeable<Variable> {
	Function *function;
	std::string name;
	std::shared_ptr<Type> type;

	Variable(const std::string &name_, std::shared_ptr<Type>, Function *);

	virtual ~Variable() {}

	size_t getSize() const {
		return type->getSize();
	}

	operator std::string() const;
};

using VregPtr = std::shared_ptr<VirtualRegister>;
using VariablePtr = std::shared_ptr<Variable>;
