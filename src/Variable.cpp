#include "Function.h"
#include "Type.h"
#include "Variable.h"
#include "Why.h"

VirtualRegister::VirtualRegister(Function &function_, const std::shared_ptr<Type> &type_):
	type(type_), function(&function_), id(function_.nextVariable++) {}

VirtualRegister::VirtualRegister(int id_, const std::shared_ptr<Type> &type_):
	type(type_), id(id_) {}

std::shared_ptr<VirtualRegister> VirtualRegister::init() {
	auto ptr = shared_from_this();
	if (function != nullptr)
		function->virtualRegisters.insert(ptr);
	return ptr;
}

std::string VirtualRegister::regOrID(bool colored, bool with_type) const {
	std::string reg_string;
	if (colored)
		reg_string = reg == -1? "\e[1m%" + std::to_string(id) + "\e[22m" : Why::coloredRegister(reg);
	else
		reg_string = reg == -1? "%" + std::to_string(id) : "$" + Why::registerName(reg);
#ifdef DEFAULT_TO_VOID
	if (type == nullptr)
		return with_type? reg_string + "{v}" : reg_string;
#else
	if (type == nullptr)
		return reg_string;
#endif
	return with_type? reg_string + std::string(OperandType(*type)) : reg_string;
}

bool VirtualRegister::special() const {
	return Why::isSpecialPurpose(reg);
}

size_t VirtualRegister::getSize() const {
	return type->getSize();
}

VirtualRegister & VirtualRegister::setReg(int new_reg, bool bypass) {
	if (!bypass && (new_reg == 0 || new_reg == 1))
		throw std::out_of_range("Invalid register: " + std::to_string(new_reg));
	reg = new_reg;
	return *this;
}

VirtualRegister * VirtualRegister::setType(const Type &type_) {
	type = std::shared_ptr<Type>(type_.copy()->setLvalue(false));
	return this;
}

Variable::Variable(const std::string &name_, const std::shared_ptr<Type> &type_, Function &function_):
	VirtualRegister(function_, type_), name(name_) {}

Variable::Variable(const std::string &name_, const std::shared_ptr<Type> &type_):
	VirtualRegister(-1, type_), name(name_) {}

Variable::operator std::string() const {
	return name + ": " + std::string(*type);
}

VirtualRegister * Variable::setType(const Type &type_) {
	type = std::shared_ptr<Type>(type_.copy()->setLvalue(true));
	return this;
}

std::ostream & operator<<(std::ostream &os, const VirtualRegister &vreg) {
	return os << std::string(vreg);
}
