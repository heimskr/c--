#include "Function.h"
#include "Variable.h"
#include "Why.h"

VirtualRegister::VirtualRegister(Function &function): id(function.nextVariable++) {}

std::string VirtualRegister::regOrID() const {
	return reg == -1? "%" + std::to_string(id) : "$" + Why::registerName(reg);
}
