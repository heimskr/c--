#include "Function.h"
#include "Type.h"
#include "Variable.h"
#include "Why.h"

VirtualRegister::VirtualRegister(Function &function): id(function.nextVariable++) {
	function.virtualRegisters.insert(shared_from_this());
}

std::string VirtualRegister::regOrID(bool colored) const {
	if (colored)
		return reg == -1? "\e[1m%" + std::to_string(id) + "\e[22m" : Why::coloredRegister(reg);
	return reg == -1? "%" + std::to_string(id) : "$" + Why::registerName(reg);
}

bool VirtualRegister::special() const {
	return Why::isSpecialPurpose(reg);
}

Variable::Variable(const std::string &name_, std::shared_ptr<Type> type_, Function *function_):
	VirtualRegister(function_? function_->nextVariable++ : -1), function(function_), name(name_), type(type_) {}

size_t Variable::getSize() const {
	return type->getSize();
}

Variable::operator std::string() const {
	return name + ": " + std::string(*type);
}

std::ostream & operator<<(std::ostream &os, const VirtualRegister &vreg) {
	return os << std::string(vreg);
}
