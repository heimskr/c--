#pragma once

#include "fixed_string.h"
#include "Immediate.h"
#include "Instruction.h"
#include "Variable.h"
#include "Why.h"

struct WhyInstruction: Instruction {
	virtual std::vector<VregPtr> getRead() { return {}; }
	virtual std::vector<VregPtr> getWritten() { return {}; }
};

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

struct RType: ThreeRegs {
	using ThreeRegs::ThreeRegs;
	std::vector<VregPtr> getRead() override {
		std::vector<VregPtr> out;
		if (leftSource)
			out.push_back(leftSource);
		if (rightSource)
			out.push_back(rightSource);
		return out;
	}
	std::vector<VregPtr> getWritten() override {
		if (destination)
			return {destination};
		return {};
	}
};

struct IType: TwoRegs, HasImmediate {
	IType(VregPtr source_, VregPtr destination_, const Immediate &imm_):
		TwoRegs(source_, destination_), HasImmediate(imm_) {}
	std::vector<VregPtr> getRead() override {
		if (source)
			return {source};
		return {};
	}
	std::vector<VregPtr> getWritten() override {
		if (destination)
			return {destination};
		return {};
	}
};

struct JType: WhyInstruction, HasSource, HasImmediate {
	bool link;
	JType(const Immediate &imm_, bool link_ = false, VregPtr source_ = nullptr):
		HasSource(source_), HasImmediate(imm_), link(link_) {}
	std::vector<VregPtr> getRead() override {
		if (source)
			return {source};
		return {};
	}
};

struct MoveInstruction: RType {
	MoveInstruction(VregPtr source_, VregPtr destination_): RType(source_, nullptr, destination_) {}
	operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " -> " + destination->regOrID()};
	}
};

struct AddRInstruction: RType {
	using RType::RType;
	operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " + " + rightSource->regOrID() + " -> " + destination->regOrID()};
	}
};

struct AddIInstruction: IType {
	using IType::IType;
	operator std::vector<std::string>() const override {
		return {source->regOrID() + " + " + stringify(imm) + " -> " + destination->regOrID()};
	}
};

struct SubRInstruction: RType {
	using RType::RType;
	operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " - " + rightSource->regOrID() + " -> " + destination->regOrID()};
	}
};

struct SubIInstruction: IType {
	using IType::IType;
	operator std::vector<std::string>() const override {
		return {source->regOrID() + " - " + stringify(imm) + " -> " + destination->regOrID()};
	}
};

struct MultRInstruction: RType {
	using RType::RType;
	operator std::vector<std::string>() const override {
		return {
			leftSource->regOrID() + " * " + rightSource->regOrID(),
			"$lo -> " + destination->regOrID()
		};
	}
};

struct MultIInstruction: IType {
	using IType::IType;
	operator std::vector<std::string>() const override {
		return {
			source->regOrID() + " * " + stringify(imm),
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

struct LuiIInstruction: IType {
	LuiIInstruction(VregPtr destination_, const Immediate &imm_): IType(nullptr, destination_, imm_) {}
	operator std::vector<std::string>() const override {
		return {"lui: " + stringify(imm) + " -> " + destination->regOrID()};
	}
};

struct LoadIInstruction: IType {
	LoadIInstruction(VregPtr destination_, const Immediate &imm_): IType(nullptr, destination_, imm_) {}
	operator std::vector<std::string>() const override {
		return {"[" + stringify(imm) + "] -> " + destination->regOrID()};
	}
};

struct LoadRInstruction: RType {
	LoadRInstruction(VregPtr source_, VregPtr destination_): RType(source_, nullptr, destination_) {}
	operator std::vector<std::string>() const override {
		return {"[" + leftSource->regOrID() + "] -> " + destination->regOrID()};
	}
};

struct StackPushInstruction: RType {
	StackPushInstruction(VregPtr source_): RType(source_, nullptr, nullptr) {}
	operator std::vector<std::string>() const override {
		return {"[ " + leftSource->regOrID()};
	}
};

struct StackPopInstruction: RType {
	StackPopInstruction(VregPtr destination_): RType(nullptr, nullptr, destination_) {}
	operator std::vector<std::string>() const override {
		return {"] " + destination->regOrID()};
	}
};

struct SizedStackPushInstruction: IType {
	SizedStackPushInstruction(VregPtr source_, const Immediate &imm_): IType(source_, nullptr, imm_) {}
	operator std::vector<std::string>() const override {
		return {"[:" + stringify(imm) + " " + source->regOrID()};
	}
};

struct SizedStackPopInstruction: IType {
	SizedStackPopInstruction(VregPtr destination_, const Immediate &imm_): IType(nullptr, destination_, imm_) {}
	operator std::vector<std::string>() const override {
		return {"]:" + stringify(imm) + " " + destination->regOrID()};
	}
};

struct JumpInstruction: JType {
	JumpInstruction(const Immediate &addr, bool link_ = false): JType(addr, link_) {}
	operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + stringify(imm)};
	}
};

struct Label: WhyInstruction {
	std::string name;
	Label(const std::string &name_): name(name_) {}
	operator std::vector<std::string>() const override {
		return {"@" + name};
	}
};

struct Comment: WhyInstruction {
	std::string comment;
	Comment(const std::string &comment_): comment(comment_) {}
	operator std::vector<std::string>() const override {
		return {"// " + comment};
	}
};

struct JumpRegisterInstruction: RType {
	bool link;
	JumpRegisterInstruction(VregPtr target, bool link_ = false):
		RType(target, nullptr, nullptr), link(link_) {}
	operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + leftSource->regOrID()};
	}
};

struct JumpRegisterConditionalInstruction: RType {
	bool link;
	JumpRegisterConditionalInstruction(VregPtr target, VregPtr condition, bool link_ = false):
		RType(target, condition, nullptr), link(link_) {}
	operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + leftSource->regOrID() + " if " + rightSource->regOrID()};
	}
};

struct JumpConditionalInstruction: JType {
	JumpConditionalInstruction(const Immediate &addr, VregPtr condition, bool link_ = false):
		JType(addr, link_, condition) {}
	operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + stringify(imm) + " if " + source->regOrID()};
	}
};

struct LogicalNotInstruction: RType {
	LogicalNotInstruction(VregPtr reg): RType(reg, nullptr, reg) {}
	LogicalNotInstruction(VregPtr source_, VregPtr destination_): RType(source_, nullptr, destination_) {}
	operator std::vector<std::string>() const override {
		return {"!" + leftSource->regOrID() + " -> " + destination->regOrID()};
	}
};

template <fixstr::fixed_string O>
struct CompRInstruction: RType {
	using RType::RType;
	operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " " + std::string(O) + " " + rightSource->regOrID() + " -> " +
			destination->regOrID()};
	}
};

struct LtRInstruction:  CompRInstruction<"<">  { using CompRInstruction::CompRInstruction; };
struct LteRInstruction: CompRInstruction<"<="> { using CompRInstruction::CompRInstruction; };
struct GtRInstruction:  CompRInstruction<">">  { using CompRInstruction::CompRInstruction; };
struct GteRInstruction: CompRInstruction<">="> { using CompRInstruction::CompRInstruction; };
struct EqRInstruction:  CompRInstruction<"=="> { using CompRInstruction::CompRInstruction; };
struct NeqRInstruction: CompRInstruction<"!="> { using CompRInstruction::CompRInstruction; };
