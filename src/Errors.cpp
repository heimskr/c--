#include "Errors.h"
#include "Function.h"
#include "Variable.h"

NotOnStackError::NotOnStackError(VariablePtr variable_):
	std::runtime_error("Not on stack: " + variable_->name +
		(variable_->function? " in function " + variable_->function->name : "")),
	variable(variable_) {}
