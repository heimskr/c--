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

struct HasTwoSources {
	VregPtr leftSource, rightSource;
	HasTwoSources(VregPtr left_source, VregPtr right_source): leftSource(left_source), rightSource(right_source) {}
};

struct HasImmediate {
	Immediate imm;
	HasImmediate(const Immediate &imm_): imm(imm_) {}
};

struct TwoRegs: WhyInstruction, HasSource, HasDestination {
	TwoRegs(VregPtr source_, VregPtr destination_): HasSource(source_), HasDestination(destination_) {}
};

struct ThreeRegs: WhyInstruction, HasTwoSources, HasDestination {
	ThreeRegs(VregPtr left_source, VregPtr right_source, VregPtr destination_):
		HasTwoSources(left_source, right_source), HasDestination(destination_) {}
};

struct IType: TwoRegs, HasImmediate {
	IType(VregPtr source_, VregPtr destination_, const Immediate &imm_):
		TwoRegs(source_, destination_), HasImmediate(imm_) {}
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

struct MultRInstruction: ThreeRegs {
	using ThreeRegs::ThreeRegs;
	operator std::vector<std::string>() const override {
		return {
			leftSource->regOrID() + " * " + rightSource->regOrID(),
			"$lo -> " + destination->regOrID()
		};
	}
};

struct StoreIInstruction: IType {
	StoreIInstruction(VregPtr source_, const Immediate &imm_): IType(source_, nullptr, imm_) {}
	operator std::vector<std::string>() const override {
		return {source->regOrID() + " -> [" + stringify(imm) + "]"};
	}
};

struct SetIInstruction: IType {
	SetIInstruction(VregPtr destination_, const Immediate &imm_): IType(nullptr, destination_, imm_) {}
	operator std::vector<std::string>() const override {
		return {stringify(imm) + " -> " + destination->regOrID()};
	}
};

struct LoadIInstruction: IType {
	LoadIInstruction(VregPtr destination_, const Immediate &imm_): IType(nullptr, destination_, imm_) {}
	operator std::vector<std::string>() const override {
		return {"[" + stringify(imm) + "] -> " + destination->regOrID()};
	}
};
