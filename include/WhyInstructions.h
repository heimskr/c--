#pragma once

#include "Instruction.h"
#include "Variable.h"

struct WhyInstruction: Instruction {};

struct HasDestination {
	VariablePtr destination;
	HasDestination(VariablePtr destination_): destination(destination_) {}
};

struct TwoRegs: WhyInstruction, HasDestination {
	VariablePtr source;
	TwoRegs(VariablePtr source_, VariablePtr destination_): HasDestination(destination_), source(source_) {}
};

struct ThreeRegs: WhyInstruction, HasDestination {
	VariablePtr leftSource, rightSource;
	ThreeRegs(VariablePtr left_source, VariablePtr right_source, VariablePtr destination_):
		HasDestination(destination_), leftSource(left_source), rightSource(right_source) {}
};

struct MoveInstruction: TwoRegs {
	using TwoRegs::TwoRegs;
	operator std::string() const override {
		return source->regOrID() + " -> " + destination->regOrID();
	}
};

struct AddRInstruction: ThreeRegs {
	using ThreeRegs::ThreeRegs;
	operator std::string() const override {
		return leftSource->regOrID() + " + " + rightSource->regOrID() + " -> " + destination->regOrID();
	}
};
