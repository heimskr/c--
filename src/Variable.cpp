#include "Function.h"
#include "Variable.h"
#include "Why.h"

VirtualRegister::VirtualRegister(Function &function): id(function.nextVariable++) {}

std::string VirtualRegister::regOrID() const {
	return reg == -1? "%" + std::to_string(id) : "$" + Why::registerName(reg);
}

Variable::Variable(const std::string &name_, std::shared_ptr<Type> type_, Function *function_):
	VirtualRegister(function_? function_->nextVariable++ : -1), function(function_), name(name_), type(type_) {}

Variable::operator std::string() const {
	return name + ": " + std::string(*type);
}
