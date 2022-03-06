#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Type.h"
#include "Variable.h"

ResolutionError::ResolutionError(const std::string &name_, const Context &context, const ASTLocation &location_):
	GenericError(location_, "Couldn't resolve " + (name_ == "$c" || name_ == "$d"? std::string(name_ == "$c"?
		"constructor" : "destructor") + " for " + (context.structName.empty()? " struct" : context.structName) :
		("symbol " + name_))),
	scope(context.scope), name(name_), structName(context.structName) {}

NotOnStackError::NotOnStackError(VariablePtr variable_, const ASTLocation &location_):
	GenericError(location_, "Not on stack: " + variable_->name +
		(variable_->function? " in function " + variable_->function->name : "")),
	variable(variable_) {}

ImplicitConversionError::ImplicitConversionError(std::shared_ptr<Type> left_, std::shared_ptr<Type> right_,
const ASTLocation &location_):
	GenericError(location_, "Cannot implicitly convert " + std::string(*left_) + " to " + std::string(*right_)),
	left(left_), right(right_) {}

ImplicitConversionError::ImplicitConversionError(const Type &left_, const Type &right_):
	ImplicitConversionError(TypePtr(left_.copy()), TypePtr(right_.copy())) {}

ImplicitConversionError::ImplicitConversionError(const Type &left_, const Type &right_, const ASTLocation &location_):
	ImplicitConversionError(TypePtr(left_.copy()), TypePtr(right_.copy()), location_) {}

NotPointerError::NotPointerError(std::shared_ptr<Type> type_, const ASTLocation &location_):
	GenericError(location_, "Not a pointer: " + std::string(*type_)), type(type_) {}

NotArrayError::NotArrayError(std::shared_ptr<Type> type_, const ASTLocation &location_):
	GenericError(location_, "Not an array: " + std::string(*type_)), type(type_) {}

NotStructError::NotStructError(std::shared_ptr<Type> type_, const ASTLocation &location_):
	GenericError(location_, "Not a struct: " + std::string(*type_)), type(type_) {}
