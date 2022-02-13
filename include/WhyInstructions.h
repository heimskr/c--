#pragma once

#include "Instruction.h"
#include "Variable.h"

struct WhyInstruction: Instruction {};

struct MoveInstruction: WhyInstruction {
	VariablePtr source, destination;
	MoveInstruction(VariablePtr source_, VariablePtr destination_): source(source_), destination(destination_) {}

	operator std::string() const override {
		return source->regOrID() + " -> " + destination->regOrID();
	}
};