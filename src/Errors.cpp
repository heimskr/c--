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

ImplicitConversionError::ImplicitConversionError(std::shared_ptr<Type> left_, std::shared_ptr<Type> right_,
const ASTLocation &location_):
	std::runtime_error("Cannot implicitly convert " + std::string(*left_) + " to " + std::string(*right_) + " at " +
	std::string(location_)), left(left_), right(right_), location(location_) {}

ImplicitConversionError::ImplicitConversionError(const Type &left_, const Type &right_):
	ImplicitConversionError(TypePtr(left_.copy()), TypePtr(right_.copy())) {}

ImplicitConversionError::ImplicitConversionError(const Type &left_, const Type &right_, const ASTLocation &location_):
	ImplicitConversionError(TypePtr(left_.copy()), TypePtr(right_.copy()), location_) {}

NotPointerError::NotPointerError(std::shared_ptr<Type> type_):
	std::runtime_error("Not a pointer: " + std::string(*type_)), type(type_) {}

NotArrayError::NotArrayError(std::shared_ptr<Type> type_):
	std::runtime_error("Not an array: " + std::string(*type_)), type(type_) {}

NotStructError::NotStructError(std::shared_ptr<Type> type_):
	std::runtime_error("Not a struct: " + std::string(*type_)), type(type_) {}
