#include "Function.h"
#include "Variable.h"
#include "Why.h"

Variable::Variable(Function &function): id(function.nextVariable++) {}
std::string Variable::regOrID() const {
	return reg == -1? std::to_string(id) : Why::registerName(reg);
}
