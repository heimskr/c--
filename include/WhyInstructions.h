#pragma once

#include "Instruction.h"
#include "Variable.h"
#include "Why.h"

struct WhyInstruction: Instruction {};

struct HasDestination {
	VregPtr destination;
	HasDestination(VregPtr destination_): destination(destination_) {}
};

struct HasSource {
	VregPtr source;
	HasSource(VregPtr source_): source(source_) {}
};

struct HasImmediate {
	Immediate imm;
	HasImmediate(const Immediate &imm_): imm(imm_) {}
};

struct TwoRegs: WhyInstruction, HasSource, HasDestination {
	TwoRegs(VregPtr source_, VregPtr destination_): HasSource(source_), HasDestination(destination_) {}
};

struct ThreeRegs: WhyInstruction, HasDestination {
	VregPtr leftSource, rightSource;
	ThreeRegs(VregPtr left_source, VregPtr right_source, VregPtr destination_):
		HasDestination(destination_), leftSource(left_source), rightSource(right_source) {}
};

struct MoveInstruction: TwoRegs {
	using TwoRegs::TwoRegs;
	operator std::vector<std::string>() const override {
		return {source->regOrID() + " -> " + destination->regOrID()};
	}
};

struct AddRInstruction: ThreeRegs {
	using ThreeRegs::ThreeRegs;
	operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " + " + rightSource->regOrID() + " -> " + destination->regOrID()};
	}
};

struct StoreIInstruction: WhyInstruction, HasSource, HasImmediate {
	StoreIInstruction(VregPtr source_, const Immediate &imm_): HasSource(source_), HasImmediate(imm_) {}
	operator std::vector<std::string>() const override {
		return {source->regOrID() + " -> [" + stringify(imm) + "]"};
	}
};
