#pragma once

#include <memory>

#include "Expr.h"
#include "Instruction.h"
#include "Variable.h"

class Function;
struct WhyInstruction;

struct CmmInstruction: Instruction {
	virtual void convert(Function &) = 0;
};

struct AssignInstruction: CmmInstruction {
	VariablePtr destination;
	ExprPtr expression;
	AssignInstruction(VariablePtr destination_, ExprPtr expression_):
		destination(destination_), expression(expression_) {}
};
