#pragma once

#include <string>

#include "Function.h"

struct Variable {
	int id;
	int reg = -1;

	Variable(int id_): id(id_) {}
	Variable(Function &function): id(function.nextVariable++) {}
};
