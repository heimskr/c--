#pragma once

#include "Checkable.h"
#include "fixed_string.h"
#include "Immediate.h"
#include "Instruction.h"
#include "Variable.h"
#include "Why.h"

static inline std::string o(const std::string &str) {
	return " \e[2m" + str + "\e[22m ";
}

struct WhyInstruction: Instruction, Checkable, std::enable_shared_from_this<WhyInstruction> {
	virtual std::vector<VregPtr> getRead() { return {}; }
	virtual std::vector<VregPtr> getWritten() { return {}; }
	virtual bool isTerminal() const { return false; }

	template <typename T>
	std::shared_ptr<T> ptrcast() {
		return std::dynamic_pointer_cast<T>(shared_from_this());
	}

	template <typename T>
	std::shared_ptr<const T> ptrcast() const {
		return std::dynamic_pointer_cast<const T>(shared_from_this());
	}

	/** Attempts to replace a variable read by the instruction with another variable. Should be overridden by any
	 *  instruction that reads from a variable. */
	virtual bool replaceRead(VregPtr, VregPtr) {
		return false;
	}

	virtual bool canReplaceRead(VregPtr) const {
		return false;
	}

	/** Attempts to replace a variable written by the instruction with another variable. Should be overridden by any
	 *  instruction that writes to a variable. */
	virtual bool replaceWritten(VregPtr, VregPtr) {
		return false;
	}

	virtual bool canReplaceWritten(VregPtr) const {
		return false;
	}

	virtual bool doesRead(VregPtr) const {
		return false;
	}

	virtual bool doesWrite(VregPtr) const {
		return false;
	}
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

	bool replaceRead(VregPtr from, VregPtr to) override {
		if (source != from)
			return false;
		source = to;
		return true;
	}

	bool canReplaceRead(VregPtr var) const override {
		return var == source;
	}

	bool replaceWritten(VregPtr from, VregPtr to) override {
		if (destination != from)
			return false;
		destination = to;
		return true;
	}

	bool canReplaceWritten(VregPtr var) const override {
		return destination == var;
	}

	bool doesRead(VregPtr var) const override {
		return source == var;
	}

	bool doesWrite(VregPtr var) const override {
		return destination == var;
	}
};

struct ThreeRegs: WhyInstruction, HasTwoSources, HasDestination {
	ThreeRegs(VregPtr left_source, VregPtr right_source, VregPtr destination_):
		HasTwoSources(left_source, right_source), HasDestination(destination_) {}

	bool replaceRead(VregPtr from, VregPtr to) override {
		if (leftSource != from && rightSource != from)
			return false;
		if (leftSource == from)
			leftSource = to;
		if (rightSource == from)
			rightSource = to;
		return true;
	}

	bool canReplaceRead(VregPtr var) const override {
		return var == leftSource || var == rightSource;
	}

	bool replaceWritten(VregPtr from, VregPtr to) override {
		if (destination != from)
			return false;
		destination = to;
		return true;
	}

	bool canReplaceWritten(VregPtr var) const override {
		return destination == var;
	}

	bool doesRead(VregPtr var) const override {
		return leftSource == var || rightSource == var;
	}

	bool doesWrite(VregPtr var) const override {
		return destination == var;
	}
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
	std::vector<std::string> colored() const override {
		return {leftSource->regOrID(true) + o("->") + destination->regOrID(true)};
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
	std::vector<std::string> colored() const override {
		return {
			leftSource->regOrID(true) + o("*") + rightSource->regOrID(true),
			Why::coloredRegister(Why::loOffset) + o("->") + destination->regOrID(true)
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
	std::vector<std::string> colored() const override {
		return {
			source->regOrID(true) + o("*") + stringify(imm, true),
			Why::coloredRegister(Why::loOffset) + o("->") + destination->regOrID(true)
		};
	}
};

struct SizedInstruction {
	size_t size;
	SizedInstruction(size_t size_): size(size_) {}

	protected:
		std::string suffix(bool colored = false) const {
			switch (size) {
				case 1: return colored? " \e[2m/b\e[22m" : " /b";
				case 2: return colored? " \e[2m/s\e[22m" : " /s";
				case 4: return colored? " \e[2m/h\e[22m" : " /h";
				case 8: return "";
				default: throw std::out_of_range("Invalid instruction size: " + std::to_string(size));
			}
		}
};

struct StoreIInstruction: IType, SizedInstruction {
	StoreIInstruction(VregPtr source_, const Immediate &imm_, size_t size_):
		IType(source_, nullptr, imm_), SizedInstruction(size_) {}
	operator std::vector<std::string>() const override {
		return {source->regOrID() + " -> [" + stringify(imm) + "]" + suffix()};
	}
	std::vector<std::string> colored() const override {
		return {source->regOrID(true) + " \e[2m-> [\e[22m" + stringify(imm, true) + "\e[2m]\e[22m" + suffix(true)};
	}
};

struct StoreRInstruction: RType, SizedInstruction {
	StoreRInstruction(VregPtr source_, VregPtr destination_, size_t size_):
		RType(source_, nullptr, destination_), SizedInstruction(size_) {}
	operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " -> [" + destination->regOrID() + "]" + suffix()};
	}
	std::vector<std::string> colored() const override {
		return {
			leftSource->regOrID(true) + " \e[2m-> [\e[22m" + destination->regOrID(true) + "\e[2m]\e[22m" + suffix(true)
		};
	}
};

struct SetIInstruction: IType {
	SetIInstruction(VregPtr destination_, const Immediate &imm_): IType(nullptr, destination_, imm_) {}
	SetIInstruction(VregPtr destination_, size_t imm_): IType(nullptr, destination_, int(imm_)) {
		if (!Util::inRange(imm_))
			throw std::out_of_range("Immediate value out of range: " + std::to_string(imm_));
	}
	operator std::vector<std::string>() const override {
		return {stringify(imm) + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {stringify(imm, true) + o("->") + destination->regOrID(true)};
	}
};

struct LuiIInstruction: IType {
	LuiIInstruction(VregPtr destination_, const Immediate &imm_): IType(nullptr, destination_, imm_) {}
	operator std::vector<std::string>() const override {
		return {"lui: " + stringify(imm) + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"lui: " + stringify(imm, true) + o("->") + destination->regOrID(true)};
	}
};

struct LoadIInstruction: IType, SizedInstruction {
	LoadIInstruction(VregPtr destination_, const Immediate &imm_, size_t size_):
		IType(nullptr, destination_, imm_), SizedInstruction(size_) {}
	operator std::vector<std::string>() const override {
		return {"[" + stringify(imm) + "] -> " + destination->regOrID() + suffix()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2m[\e[22m" + stringify(imm, true) + "\e[2m] ->\e[22m " + destination->regOrID(true) + suffix(true)};
	}
};

struct LoadRInstruction: RType, SizedInstruction {
	size_t size;
	LoadRInstruction(VregPtr source_, VregPtr destination_, size_t size_):
		RType(source_, nullptr, destination_), SizedInstruction(size_) {}
	operator std::vector<std::string>() const override {
		return {"[" + leftSource->regOrID() + "] -> " + destination->regOrID() + suffix()};
	}
	std::vector<std::string> colored() const override {
		return {
			"\e[2m[\e[22m" + leftSource->regOrID(true) + "\e[2m] ->\e[22m " + destination->regOrID(true) + suffix(true)
		};
	}
};

struct StackPushInstruction: RType {
	StackPushInstruction(VregPtr source_): RType(source_, nullptr, nullptr) {}
	operator std::vector<std::string>() const override {
		return {"[ " + leftSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2;1m[\e[22m " + leftSource->regOrID(true)};
	}
};

struct StackPopInstruction: RType {
	StackPopInstruction(VregPtr destination_): RType(nullptr, nullptr, destination_) {}
	operator std::vector<std::string>() const override {
		return {"] " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2;1m]\e[22m " + destination->regOrID(true)};
	}
};

struct StackStoreInstruction: RType {
	int offset;
	StackStoreInstruction(VregPtr source_, int offset_): RType(source_, nullptr, nullptr), offset(offset_) {}
	operator std::vector<std::string>() const override {
		if (offset == 0)
			return {leftSource->regOrID() + " -> [$fp]"};
		return {
			"$fp - " + std::to_string(offset) + " -> $m1",
			leftSource->regOrID() + " -> [$m1]"
		};
	}
	std::vector<std::string> colored() const override {
		if (offset == 0)
			return {
				leftSource->regOrID() + " \e[2m-> [\e[22m" + Why::coloredRegister(Why::framePointerOffset) +
					"\e[2m]\e[22m"
			};
		return {
			Why::coloredRegister(Why::framePointerOffset) + o("-") + stringify(offset, true) + o("->") +
				Why::coloredRegister(Why::assemblerOffset + 1),
			leftSource->regOrID(true) + " \e[2m-> [\e[22m" + Why::coloredRegister(Why::assemblerOffset + 1) +
				"\e[2m]\e[22m",
		};
	}
	bool operator==(const StackStoreInstruction &other) const {
		return leftSource == other.leftSource && offset == other.offset;
	}
	StackStoreInstruction & setSource(VregPtr new_source) {
		leftSource = new_source;
		return *this;
	}
};

struct StackLoadInstruction: RType {
	int offset;
	StackLoadInstruction(VregPtr destination_, int offset_): RType(nullptr, nullptr, destination_), offset(offset_) {}
	operator std::vector<std::string>() const override {
		if (offset == 0)
			return {"[$fp] -> " + destination->regOrID()};
		return {
			"$fp - " + std::to_string(offset) + " -> $m1",
			"[$m1] -> " + destination->regOrID()
		};
	}
	std::vector<std::string> colored() const override {
		if (offset == 0)
			return {
				"\e[2m[\e[22m" + Why::coloredRegister(Why::framePointerOffset) + "\e[2m] ->\e[22m " +
					destination->regOrID(true)
			};
		return {
			Why::coloredRegister(Why::framePointerOffset) + o("-") + stringify(offset, true) + o("->") +
				Why::coloredRegister(Why::assemblerOffset + 1),
			"\e[2m[\e[22m" + Why::coloredRegister(Why::assemblerOffset + 1) + "\e[2m] ->\e[22m " +
				destination->regOrID(true),
		};
	}
	bool operator==(const StackStoreInstruction &other) const {
		return destination == other.destination && offset == other.offset;
	}
};

struct SizedStackPushInstruction: IType {
	SizedStackPushInstruction(VregPtr source_, const Immediate &imm_): IType(source_, nullptr, imm_) {}
	operator std::vector<std::string>() const override {
		return {"[:" + stringify(imm) + " " + source->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2;1m[:" + stringify(imm) + "\e[22m " + source->regOrID(true)};
	}
};

struct SizedStackPopInstruction: IType {
	SizedStackPopInstruction(VregPtr destination_, const Immediate &imm_): IType(nullptr, destination_, imm_) {}
	operator std::vector<std::string>() const override {
		return {"]:" + stringify(imm) + " " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2;1m]:" + stringify(imm) + "\e[22m " + destination->regOrID(true)};
	}
};

struct JumpInstruction: JType {
	JumpInstruction(const Immediate &addr, bool link_ = false): JType(addr, link_) {}
	bool isTerminal() const override { return !link; }
	operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + stringify(imm)};
	}
	std::vector<std::string> colored() const override {
		return {"\e[1;2m" + std::string(link? "::" : ":") + "\e[22m " + stringify(imm, true)};
	}
};

struct Label: WhyInstruction {
	std::string name;
	Label(const std::string &name_): name(name_) {}
	operator std::vector<std::string>() const override {
		return {"@" + name};
	}
	std::vector<std::string> colored() const override {
		return {"\e[36m@\e[39m\e[38;5;202m" + name + "\e[39m"};
	}
};

struct Comment: WhyInstruction, Makeable<Comment> {
	std::string comment;
	Comment(const std::string &comment_): comment(comment_) {}
	operator std::vector<std::string>() const override {
		return {"// " + comment};
	}
	std::vector<std::string> colored() const override {
		return {"\e[32;2;1m// " + comment + "\e[39;22m"};
	}
};

struct JumpRegisterInstruction: RType {
	bool link;
	JumpRegisterInstruction(VregPtr target, bool link_ = false):
		RType(target, nullptr, nullptr), link(link_) {}
	bool isTerminal() const override { return !link; }
	operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + leftSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[1;2m" + std::string(link? "::" : ":") + "\e[22m " + leftSource->regOrID(true)};
	}
};

struct JumpRegisterConditionalInstruction: RType {
	bool link;
	JumpRegisterConditionalInstruction(VregPtr target, VregPtr condition, bool link_ = false):
		RType(target, condition, nullptr), link(link_) {}
	operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + leftSource->regOrID() + " if " + rightSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {
			"\e[2m" + std::string(link? "::" : ":") + "\e[22m " + leftSource->regOrID(true) + " \e[91mif\e[39m " +
				rightSource->regOrID(true)
		};
	}
};

struct JumpConditionalInstruction: JType {
	JumpConditionalInstruction(const Immediate &addr, VregPtr condition, bool link_ = false):
		JType(addr, link_, condition) {}
	operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + stringify(imm) + " if " + source->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {
			"\e[2m" + std::string(link? "::" : ":") + "\e[22m " + stringify(imm, true) + " \e[91mif\e[39m " +
				source->regOrID(true)
		};
	}
};

struct LogicalNotInstruction: RType {
	LogicalNotInstruction(VregPtr reg): RType(reg, nullptr, reg) {}
	LogicalNotInstruction(VregPtr source_, VregPtr destination_): RType(source_, nullptr, destination_) {}
	operator std::vector<std::string>() const override {
		return {"!" + leftSource->regOrID() + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2m!\e[22m" + leftSource->regOrID(true) + o("->") + destination->regOrID(true)};
	}
};

struct SextInstruction: RType {
	public:
		enum class SextType {Sext8, Sext16, Sext32};
		SextType type;
		SextInstruction(VregPtr source_, VregPtr destination_, SextType type_):
			RType(source_, nullptr, destination_), type(type_) {}
		SextInstruction(VregPtr source_, VregPtr destination_, int width):
			RType(source_, nullptr, destination_), type(getType(width)) {}
		operator std::vector<std::string>() const override {
			return {getOper() + " " + leftSource->regOrID() + " -> " + destination->regOrID()};
		}
		std::vector<std::string> colored() const override {
			return {"\e[2m" + getOper() + "\e[22m " + leftSource->regOrID(true) + o("->") + destination->regOrID(true)};
		}

	private:
		std::string getOper() const {
			switch (type) {
				case SextType::Sext8:  return "sext8";
				case SextType::Sext16: return "sext16";
				case SextType::Sext32: return "sext32";
				default: return "sext?";
			}
		}

		SextType getType(int width) {
			switch (width) {
				case 8:  return SextType::Sext8;
				case 16: return SextType::Sext16;
				case 32: return SextType::Sext32;
				default:
					throw std::invalid_argument("No sext instruction exists for bitwidth " + std::to_string(width));
			}
		}
};

template <fixstr::fixed_string O>
struct BinaryRType: RType {
	using RType::RType;
	operator std::vector<std::string>() const override {
		return {
			leftSource->regOrID() + " " + std::string(O) + " " + rightSource->regOrID() + " -> " +
				destination->regOrID()
		};
	}
	std::vector<std::string> colored() const override {
		return {
			leftSource->regOrID(true) + o(std::string(O)) + rightSource->regOrID(true) + o("->") +
				destination->regOrID(true)
		};
	}
};

struct LtRInstruction:   BinaryRType<"<">  { using BinaryRType::BinaryRType; };
struct LteRInstruction:  BinaryRType<"<="> { using BinaryRType::BinaryRType; };
struct EqRInstruction:   BinaryRType<"=="> { using BinaryRType::BinaryRType; };
struct NeqRInstruction:  BinaryRType<"!="> { using BinaryRType::BinaryRType; };
struct AddRInstruction:  BinaryRType<"+">  { using BinaryRType::BinaryRType; };
struct SubRInstruction:  BinaryRType<"-">  { using BinaryRType::BinaryRType; };
struct AndRInstruction:  BinaryRType<"&">  { using BinaryRType::BinaryRType; };
struct OrRInstruction:   BinaryRType<"|">  { using BinaryRType::BinaryRType; };
struct XorRInstruction:  BinaryRType<"^">  { using BinaryRType::BinaryRType; };
struct LandRInstruction: BinaryRType<"&&"> { using BinaryRType::BinaryRType; };
struct LorRInstruction:  BinaryRType<"||"> { using BinaryRType::BinaryRType; };
struct LxorRInstruction: BinaryRType<"^^"> { using BinaryRType::BinaryRType; };
struct DivRInstruction:  BinaryRType<"/">  { using BinaryRType::BinaryRType; };
struct ShiftLeftLogicalRInstruction:     BinaryRType<"<<">  { using BinaryRType::BinaryRType; };
struct ShiftRightArithmeticRInstruction: BinaryRType<">>">  { using BinaryRType::BinaryRType; };
struct ShiftRightLogicalRInstruction:    BinaryRType<">>>"> { using BinaryRType::BinaryRType; };

template <fixstr::fixed_string O>
struct BinaryIType: IType {
	using IType::IType;
	operator std::vector<std::string>() const override {
		return {source->regOrID() + " " + std::string(O) + " " + stringify(imm) + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {
			source->regOrID(true) + o(std::string(O)) + stringify(imm, true) + o("->") + destination->regOrID(true)
		};
	}
};

struct AddIInstruction: BinaryIType<"+"> { using BinaryIType::BinaryIType; };
struct SubIInstruction: BinaryIType<"-"> { using BinaryIType::BinaryIType; };
struct AndIInstruction: BinaryIType<"&"> { using BinaryIType::BinaryIType; };
struct DivIInstruction: BinaryIType<"/"> { using BinaryIType::BinaryIType; };

template <fixstr::fixed_string O>
struct InverseBinaryRType: RType {
	using RType::RType;
	operator std::vector<std::string>() const override {
		return {
			rightSource->regOrID() + " " + std::string(O) + " " + leftSource->regOrID() + " -> " +
				destination->regOrID()
		};
	}
	std::vector<std::string> colored() const override {
		return {
			rightSource->regOrID(true) + o(std::string(O)) + leftSource->regOrID(true) + o("->") +
				destination->regOrID(true)
		};
	}
};

struct GtRInstruction:  InverseBinaryRType<"<">  { using InverseBinaryRType::InverseBinaryRType; };
struct GteRInstruction: InverseBinaryRType<"<="> { using InverseBinaryRType::InverseBinaryRType; };

template <char O>
struct UnaryRType: RType {
	UnaryRType(VregPtr source_, VregPtr destination_): RType(source_, nullptr, destination_) {}
	operator std::vector<std::string>() const override {
		return {std::string(1, O) + leftSource->regOrID() + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {
			"\e[2m" + std::string(1, O) + "\e[22m" + leftSource->regOrID(true) + " -> " + destination->regOrID(true)
		};
	}
};

struct NotInstruction:  UnaryRType<'~'> { using UnaryRType::UnaryRType; };
struct LnotInstruction: UnaryRType<'!'> { using UnaryRType::UnaryRType; };

template <fixstr::fixed_string O>
struct UnsignedBinaryRType: RType {
	using RType::RType;
	operator std::vector<std::string>() const override {
		return {
			leftSource->regOrID() + " " + std::string(O) + " " + rightSource->regOrID() + " -> " +
				destination->regOrID() + " /u"
		};
	}
	std::vector<std::string> colored() const override {
		return {
			leftSource->regOrID(true) + o(std::string(O)) + rightSource->regOrID(true) + o("->") +
				destination->regOrID(true) + " \e[2m/u\e[22m"
		};
	}
};

struct DivuRInstruction: UnsignedBinaryRType<"/"> { using UnsignedBinaryRType::UnsignedBinaryRType; };

template <fixstr::fixed_string O>
struct UnsignedBinaryIType: IType {
	using IType::IType;
	operator std::vector<std::string>() const override {
		return {
			source->regOrID() + " " + std::string(O) + " " + stringify(imm) + " -> " + destination->regOrID() + "/u"
		};
	}
	std::vector<std::string> colored() const override {
		return {
			source->regOrID(true) + o(std::string(O)) + stringify(imm, true) + o("->") + destination->regOrID(true)
				+ " \e[2m/u\e[22m"
		};
	}
};

struct DivuIInstruction: UnsignedBinaryIType<"/"> { using UnsignedBinaryIType::UnsignedBinaryIType; };
