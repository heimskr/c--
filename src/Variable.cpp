#include <iostream>
#include "Function.h"
#include "Type.h"
#include "Variable.h"
#include "Why.h"

VirtualRegister::VirtualRegister(Function &function_, std::shared_ptr<Type> type_):
	function(&function_), id(function_.nextVariable++), type(type_) {}

VirtualRegister::VirtualRegister(int id_, std::shared_ptr<Type> type_):
	id(id_), type(type_) {}

std::shared_ptr<VirtualRegister> VirtualRegister::init() {
	const auto ptr = shared_from_this();
	if (function)
		function->virtualRegisters.insert(ptr);
	return ptr;
}

std::string VirtualRegister::regOrID(bool colored) const {
	if (colored)
		return reg == -1? "\e[1m%" + std::to_string(id) + "\e[22m" : Why::coloredRegister(reg);
	return reg == -1? "%" + std::to_string(id) : "$" + Why::registerName(reg);
}

bool VirtualRegister::special() const {
	return Why::isSpecialPurpose(reg);
}

Variable::Variable(const std::string &name_, std::shared_ptr<Type> type_, Function &function_):
	VirtualRegister(function_, type_), name(name_) {}

Variable::Variable(const std::string &name_, std::shared_ptr<Type> type_):
	VirtualRegister(-1, type_), name(name_) {}

size_t Variable::getSize() const {
	return type->getSize();
}

Variable::operator std::string() const {
	return name + ": " + std::string(*type);
}

std::ostream & operator<<(std::ostream &os, const VirtualRegister &vreg) {
	return os << std::string(vreg);
}
