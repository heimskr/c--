#pragma once

#include <memory>

#include "Expr.h"
#include "Variable.h"

struct WhyInstruction;

struct CmmInstruction: Instruction {
	virtual std::unique_ptr<WhyInstruction> convert() = 0;
};

struct AssignInstruction: CmmInstruction {
	Variable destination;
	Expr expression;
	AssignInstruction(const Variable &destination_, const Expr &expression_):
		destination(destination_), expression(expression_) {}
};
