#include "Errors.h"
#include "Function.h"
#include "Type.h"
#include "Variable.h"

NotOnStackError::NotOnStackError(VariablePtr variable_):
	std::runtime_error("Not on stack: " + variable_->name +
		(variable_->function? " in function " + variable_->function->name : "")),
	variable(variable_) {}

ImplicitConversionError::ImplicitConversionError(std::shared_ptr<Type> left_, std::shared_ptr<Type> right_):
	std::runtime_error("Cannot implicitly convert " + std::string(*left_) + " to " + std::string(*right_)),
	left(left_), right(right_) {}

ImplicitConversionError::ImplicitConversionError(const Type &left_, const Type &right_):
	ImplicitConversionError(TypePtr(left_.copy()), TypePtr(right_.copy())) {}
