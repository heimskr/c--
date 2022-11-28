#pragma once

#include <climits>

#include "Checkable.h"
#include "Enums.h"
#include "Immediate.h"
#include "Instruction.h"
#include "Variable.h"
#include "Why.h"
#include "fixed_string.h"

static inline std::string o(const std::string &str) {
	return " \e[2m" + str + "\e[22m ";
}

struct WhyInstruction: Instruction, Checkable, std::enable_shared_from_this<WhyInstruction> {
	virtual std::vector<VregPtr> getRead() { return {}; }
	virtual std::vector<VregPtr> getWritten() { return {}; }
	virtual bool isTerminal() const { return false; }
	virtual bool enableDebug() const { return true; }

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
	virtual bool replaceRead(const VregPtr &, const VregPtr &) {
		return false;
	}

	virtual bool canReplaceRead(const VregPtr &) const {
		return false;
	}

	/** Attempts to replace a variable written by the instruction with another variable. Should be overridden by any
	 *  instruction that writes to a variable. */
	virtual bool replaceWritten(const VregPtr &, const VregPtr &) {
		return false;
	}

	virtual bool canReplaceWritten(const VregPtr &) const {
		return false;
	}

	virtual bool doesRead(const VregPtr &) const {
		return false;
	}

	virtual bool doesWrite(const VregPtr &) const {
		return false;
	}
};

struct HasDestination {
	VregPtr destination;
	explicit HasDestination(VregPtr destination_): destination(std::move(destination_)) {}
};

struct HasSource {
	VregPtr source;
	explicit HasSource(VregPtr source_): source(std::move(source_)) {}
};

struct HasTwoSources {
	VregPtr leftSource, rightSource;
	HasTwoSources(VregPtr left_source, VregPtr right_source):
		leftSource(std::move(left_source)), rightSource(std::move(right_source)) {}
};

struct HasImmediate {
	TypedImmediate imm;
	explicit HasImmediate(TypedImmediate imm_):
		imm(std::move(imm_)) {}
};

struct TwoRegs: WhyInstruction, HasSource, HasDestination {
	TwoRegs(VregPtr source_, VregPtr destination_):
		HasSource(std::move(source_)), HasDestination(std::move(destination_)) {}

	bool replaceRead(const VregPtr &from, const VregPtr &to) override {
		if (source != from)
			return false;
		source = to;
		return true;
	}

	bool canReplaceRead(const VregPtr &var) const override {
		return var == source;
	}

	bool replaceWritten(const VregPtr &from, const VregPtr &to) override {
		if (destination != from)
			return false;
		destination = to;
		return true;
	}

	bool canReplaceWritten(const VregPtr &var) const override {
		return destination == var;
	}

	bool doesRead(const VregPtr &var) const override {
		return source == var;
	}

	bool doesWrite(const VregPtr &var) const override {
		return destination == var;
	}
};

struct ThreeRegs: WhyInstruction, HasTwoSources, HasDestination {
	ThreeRegs(VregPtr left_source, VregPtr right_source, VregPtr destination_):
		HasTwoSources(std::move(left_source), std::move(right_source)),
		HasDestination(std::move(destination_)) {}

	bool replaceRead(const VregPtr &from, const VregPtr &to) override {
		if (leftSource != from && rightSource != from)
			return false;
		if (leftSource == from)
			leftSource = to;
		if (rightSource == from)
			rightSource = to;
		return true;
	}

	bool canReplaceRead(const VregPtr &var) const override {
		return var == leftSource || var == rightSource;
	}

	bool replaceWritten(const VregPtr &from, const VregPtr &to) override {
		if (destination != from)
			return false;
		destination = to;
		return true;
	}

	bool canReplaceWritten(const VregPtr &var) const override {
		return destination == var;
	}

	bool doesRead(const VregPtr &var) const override {
		return leftSource == var || rightSource == var;
	}

	bool doesWrite(const VregPtr &var) const override {
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
	IType(VregPtr source_, VregPtr destination_, TypedImmediate imm_):
		TwoRegs(std::move(source_), std::move(destination_)), HasImmediate(std::move(imm_)) {}

	IType(VregPtr source_, VregPtr destination_, OperandType type_, int imm_):
		TwoRegs(std::move(source_), std::move(destination_)), HasImmediate({std::move(type_), imm_}) {}

	IType(VregPtr source_, VregPtr destination_, OperandType type_, size_t imm_):
		TwoRegs(std::move(source_), std::move(destination_)), HasImmediate({std::move(type_), static_cast<int>(imm_)}) {
		if (UINT32_MAX < imm_)
			throw std::out_of_range("Immediate value out of range: " + std::to_string(imm_));
	}

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

	explicit JType(TypedImmediate imm_, bool link_ = false, VregPtr source_ = nullptr):
		HasSource(std::move(source_)), HasImmediate(std::move(imm_)), link(link_) {}

	std::vector<VregPtr> getRead() override {
		if (source)
			return {source};
		return {};
	}
};

struct MoveInstruction: RType {
	MoveInstruction(VregPtr source_, VregPtr destination_):
		RType(std::move(source_), nullptr, std::move(destination_)) {}

	explicit operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " -> " + destination->regOrID()};
	}

	std::vector<std::string> colored() const override {
		return {leftSource->regOrID(true) + o("->") + destination->regOrID(true)};
	}
};

struct MultRInstruction: RType {
	using RType::RType;

	explicit operator std::vector<std::string>() const override {
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

struct BareMultRInstruction: RType {
	BareMultRInstruction(const VregPtr &rs_, const VregPtr &rt_): RType(rs_, rt_, nullptr) {}
	explicit operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " * " + rightSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {leftSource->regOrID(true) + o("*") + rightSource->regOrID(true)};
	}
};

struct MultIInstruction: IType {
	using IType::IType;
	explicit operator std::vector<std::string>() const override;
	std::vector<std::string> colored() const override;
};

struct BareMultIInstruction: IType {
	BareMultIInstruction(VregPtr rs_, TypedImmediate imm_):
		IType(std::move(rs_), nullptr, std::move(imm_)) {}

	explicit operator std::vector<std::string>() const override {
		return {source->regOrID() + " * " + stringify(imm)};
	}

	std::vector<std::string> colored() const override {
		return {source->regOrID(true) + o("*") + stringify(imm, true)};
	}
};

struct StoreIInstruction: IType {
	StoreIInstruction(VregPtr source_, TypedImmediate imm_):
		IType(std::move(source_), nullptr, std::move(imm_)) {}

	explicit operator std::vector<std::string>() const override {
		return {source->regOrID() + " -> [" + stringify(imm) + "]"};
	}

	std::vector<std::string> colored() const override {
		return {source->regOrID(true) + " \e[2m-> [\e[22m" + stringify(imm, true) + "\e[2m]\e[22m"};
	}
};

struct StoreRInstruction: RType {
	StoreRInstruction(VregPtr source_, VregPtr address_):
		RType(std::move(source_), std::move(address_), nullptr) {}

	explicit operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " -> [" + rightSource->regOrID() + "]"};
	}

	std::vector<std::string> colored() const override {
		return {
			leftSource->regOrID(true) + " \e[2m-> [\e[22m" + rightSource->regOrID(true) + "\e[2m]\e[22m"
		};
	}
};

struct SetIInstruction: IType {
	SetIInstruction(VregPtr destination_, TypedImmediate imm_):
		IType(nullptr, std::move(destination_), std::move(imm_)) {}

	explicit operator std::vector<std::string>() const override {
		return {stringify(imm) + " -> " + destination->regOrID()};
	}

	std::vector<std::string> colored() const override {
		return {stringify(imm, true) + o("->") + destination->regOrID(true)};
	}
};

struct LuiIInstruction: IType {
	LuiIInstruction(VregPtr destination_, TypedImmediate imm_):
		IType(nullptr, std::move(destination_), std::move(imm_)) {}

	explicit operator std::vector<std::string>() const override {
		return {"lui: " + stringify(imm) + " -> " + destination->regOrID()};
	}

	std::vector<std::string> colored() const override {
		return {"lui: " + stringify(imm, true) + o("->") + destination->regOrID(true)};
	}
};

struct LoadIInstruction: IType {
	LoadIInstruction(VregPtr destination_, TypedImmediate imm_):
		IType(nullptr, std::move(destination_), std::move(imm_)) {}

	explicit operator std::vector<std::string>() const override {
		return {"[" + stringify(imm) + "] -> " + destination->regOrID()};
	}

	std::vector<std::string> colored() const override {
		return {"\e[2m[\e[22m" + stringify(imm, true) + "\e[2m] ->\e[22m " + destination->regOrID(true)};
	}
};

struct LoadIndirectIInstruction: IType {
	LoadIndirectIInstruction(VregPtr destination_, TypedImmediate imm_):
		IType(nullptr, std::move(destination_), std::move(imm_)) {}

	explicit operator std::vector<std::string>() const override {
		return {"[" + stringify(imm) + "] -> [" + destination->regOrID() + "]"};
	}

	std::vector<std::string> colored() const override {
		return {
			"\e[2m[\e[22m" + stringify(imm, true) + "\e[2m] -> [\e[22m" + destination->regOrID(true) + "\e[2m]\e[22m"
		};
	}
};

struct LoadRInstruction: RType {
	LoadRInstruction(VregPtr source_, VregPtr destination_):
		RType(std::move(source_), nullptr, std::move(destination_)) {}

	explicit operator std::vector<std::string>() const override {
		return {"[" + leftSource->regOrID() + "] -> " + destination->regOrID()};
	}

	std::vector<std::string> colored() const override {
		return {
			"\e[2m[\e[22m" + leftSource->regOrID(true) + "\e[2m] ->\e[22m " + destination->regOrID(true)
		};
	}
};

struct CopyRInstruction: RType {
	CopyRInstruction(VregPtr source_, VregPtr destination_):
		RType(std::move(source_), nullptr, std::move(destination_)) {}

	explicit operator std::vector<std::string>() const override {
		return {"[" + leftSource->regOrID() + "] -> [" + destination->regOrID() + "]"};
	}

	std::vector<std::string> colored() const override {
		return {
			"\e[2m[\e[22m" + leftSource->regOrID(true) + "\e[2m] -> [\e[22m" + destination->regOrID(true) +
				"\e[2m]\e[22m"
		};
	}
};

struct StackPushInstruction: RType {
	explicit StackPushInstruction(const VregPtr &source_): RType(source_, nullptr, nullptr) {}
	explicit operator std::vector<std::string>() const override {
		return {"[ " + leftSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2;1m[\e[22m " + leftSource->regOrID(true)};
	}
};

struct StackPopInstruction: RType {
	explicit StackPopInstruction(const VregPtr &destination_): RType(nullptr, nullptr, destination_) {}
	explicit operator std::vector<std::string>() const override {
		return {"] " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2;1m]\e[22m " + destination->regOrID(true)};
	}
};

struct StackStoreInstruction: RType {
	int offset;
	StackStoreInstruction(const VregPtr &source_, int offset_): RType(source_, nullptr, nullptr), offset(offset_) {}
	explicit operator std::vector<std::string>() const override {
		if (offset == 0)
			return {leftSource->regOrID() + " -> [$fp]"};
		return {
			"$fp{v} - " + std::to_string(offset) + "{v} -> $m1",
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
			Why::coloredRegister(Why::framePointerOffset) + "{v}" + o("-") +
				stringify(TypedImmediate(OperandType::VOID, offset), true) + o("->") +
				Why::coloredRegister(Why::assemblerOffset + 1),
			leftSource->regOrID(true) + " \e[2m-> [\e[22m" + Why::coloredRegister(Why::assemblerOffset + 1) +
				"\e[2m]\e[22m"
		};
	}

	bool operator==(const StackStoreInstruction &other) const {
		return leftSource == other.leftSource && offset == other.offset;
	}

	StackStoreInstruction & setSource(const VregPtr &new_source) {
		leftSource = new_source;
		return *this;
	}
};

struct StackLoadInstruction: RType {
	int offset;
	StackLoadInstruction(const VregPtr &destination_, int offset_):
		RType(nullptr, nullptr, destination_), offset(offset_) {}
	explicit operator std::vector<std::string>() const override {
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
			Why::coloredRegister(Why::framePointerOffset) + "{v}" + o("-") +
				stringify(TypedImmediate(OperandType::VOID, offset), true) + o("->") +
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
	SizedStackPushInstruction(const VregPtr &source_, const TypedImmediate &imm_): IType(source_, nullptr, imm_) {}
	explicit operator std::vector<std::string>() const override {
		return {"[:" + stringify(imm) + " " + source->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2;1m[:" + stringify(imm) + "\e[22m " + source->regOrID(true)};
	}
};

struct SizedStackPopInstruction: IType {
	SizedStackPopInstruction(const VregPtr &destination_, const TypedImmediate &imm_): IType(nullptr, destination_, imm_) {}
	explicit operator std::vector<std::string>() const override {
		return {"]:" + stringify(imm) + " " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[2;1m]:" + stringify(imm) + "\e[22m " + destination->regOrID(true)};
	}
};

struct Conditional {
	Condition condition;
	explicit Conditional(Condition condition_): condition(condition_) {}
	[[nodiscard]] std::string conditionPrefix() const {
		switch (condition) {
			case Condition::Zero:     return "0";
			case Condition::Negative: return "-";
			case Condition::Positive: return "+";
			case Condition::Nonzero:  return "*";
			default: return "";
		}
	}
};

struct JumpInstruction: JType, Conditional {
	explicit JumpInstruction(TypedImmediate addr, bool link_ = false, Condition condition_ = Condition::None):
		JType(std::move(addr), link_), Conditional(condition_) {}

	bool isTerminal() const override { return !link; }

	explicit operator std::vector<std::string>() const override {
		return {conditionPrefix() + std::string(link? "::" : ":") + " " + stringify(imm)};
	}

	std::vector<std::string> colored() const override {
		return {"\e[1;2m" + conditionPrefix() + std::string(link? "::" : ":") + "\e[22m " + stringify(imm, true)};
	}
};

struct Label: WhyInstruction {
	std::string name;
	bool enableDebug() const override { return false; }
	explicit Label(const std::string &name_): name(name_) {}
	explicit operator std::vector<std::string>() const override {
		return {"@" + name};
	}
	std::vector<std::string> colored() const override {
		return {"\e[36m@\e[39m\e[38;5;202m" + name + "\e[39m"};
	}
};

struct Comment: WhyInstruction, Makeable<Comment> {
	std::string comment;
	explicit Comment(const std::string &comment_): comment(comment_) {}
	explicit operator std::vector<std::string>() const override {
		return {"// " + comment};
	}
	std::vector<std::string> colored() const override {
		return {"\e[32;2;1m// " + comment + "\e[39;22m"};
	}
};

struct JumpRegisterInstruction: RType, Conditional {
	bool link;
	explicit JumpRegisterInstruction(const VregPtr &target, bool link_ = false, Condition condition_ = Condition::None):
		RType(target, nullptr, nullptr), Conditional(condition_), link(link_) {}
	bool isTerminal() const override { return !link; }
	explicit operator std::vector<std::string>() const override {
		return {conditionPrefix() + std::string(link? "::" : ":") + " " + leftSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[1;2m" + conditionPrefix() + std::string(link? "::" : ":") + "\e[22m " + leftSource->regOrID(true)};
	}
};

struct JumpRegisterConditionalInstruction: RType {
	bool link;

	JumpRegisterConditionalInstruction(VregPtr target, VregPtr condition, bool link_ = false):
		RType(std::move(target), std::move(condition), nullptr), link(link_) {}

	explicit operator std::vector<std::string>() const override {
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
	JumpConditionalInstruction(TypedImmediate addr, VregPtr condition, bool link_ = false):
		JType(std::move(addr), link_, std::move(condition)) {}

	explicit operator std::vector<std::string>() const override {
		return {std::string(link? "::" : ":") + " " + stringify(imm) + " if " + source->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {
			"\e[2m" + std::string(link? "::" : ":") + "\e[22m " + stringify(imm, true) + " \e[91mif\e[39m " +
				source->regOrID(true)
		};
	}
};

struct SextInstruction: RType {
	public:
		enum class SextType {Sext8, Sext16, Sext32};
		SextType type;
		SextInstruction(const VregPtr &source_, const VregPtr &destination_, SextType type_):
			RType(source_, nullptr, destination_), type(type_) {}
		SextInstruction(const VregPtr &source_, const VregPtr &destination_, int width):
			RType(source_, nullptr, destination_), type(getType(width)) {}
		explicit operator std::vector<std::string>() const override {
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

		static SextType getType(int width) {
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
	explicit operator std::vector<std::string>() const override {
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

struct SlRInstruction:    BinaryRType<"<">   { using BinaryRType::BinaryRType; };
struct SleRInstruction:   BinaryRType<"<=">  { using BinaryRType::BinaryRType; };
struct SeqRInstruction:   BinaryRType<"==">  { using BinaryRType::BinaryRType; };
struct AddRInstruction:   BinaryRType<"+">   { using BinaryRType::BinaryRType; };
struct SubRInstruction:   BinaryRType<"-">   { using BinaryRType::BinaryRType; };
struct AndRInstruction:   BinaryRType<"&">   { using BinaryRType::BinaryRType; };
struct OrRInstruction:    BinaryRType<"|">   { using BinaryRType::BinaryRType; };
struct XorRInstruction:   BinaryRType<"x">   { using BinaryRType::BinaryRType; };
struct NandRInstruction:  BinaryRType<"~&">  { using BinaryRType::BinaryRType; };
struct NorRInstruction:   BinaryRType<"~|">  { using BinaryRType::BinaryRType; };
struct XnorRInstruction:  BinaryRType<"~x">  { using BinaryRType::BinaryRType; };
struct LandRInstruction:  BinaryRType<"&&">  { using BinaryRType::BinaryRType; };
struct LorRInstruction:   BinaryRType<"||">  { using BinaryRType::BinaryRType; };
struct LxorRInstruction:  BinaryRType<"xx">  { using BinaryRType::BinaryRType; };
struct LnandRInstruction: BinaryRType<"~&&"> { using BinaryRType::BinaryRType; };
struct LnorRInstruction:  BinaryRType<"~||"> { using BinaryRType::BinaryRType; };
struct LxnorRInstruction: BinaryRType<"~xx"> { using BinaryRType::BinaryRType; };
struct DivRInstruction:   BinaryRType<"/">   { using BinaryRType::BinaryRType; };
struct ModRInstruction:   BinaryRType<"%">   { using BinaryRType::BinaryRType; };
struct ShiftLeftLogicalRInstruction:     BinaryRType<"<<">  { using BinaryRType::BinaryRType; };
struct ShiftRightArithmeticRInstruction: BinaryRType<">>">  { using BinaryRType::BinaryRType; };
struct ShiftRightLogicalRInstruction:    BinaryRType<">>>"> { using BinaryRType::BinaryRType; };

struct SneqRInstruction: RType {
	using RType::RType;
	explicit operator std::vector<std::string>() const override {
		return {
			leftSource->regOrID() + " == " + rightSource->regOrID() + " -> " + destination->regOrID(),
			"!" + destination->regOrID() + " -> " + destination->regOrID()
		};
	}
	std::vector<std::string> colored() const override {
		return {
			leftSource->regOrID(true) + o("==") + rightSource->regOrID(true) + o("->") + destination->regOrID(true),
			"\e[2m!\e[22m" + destination->regOrID(true) + o("->") + destination->regOrID(true)
		};
	}
};

template <fixstr::fixed_string O>
struct BinaryIType: IType {
	using IType::IType;
	explicit operator std::vector<std::string>() const override {
		return {source->regOrID() + " " + std::string(O) + " " + stringify(imm) + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {
			source->regOrID(true) + o(std::string(O)) + stringify(imm, true) + o("->") + destination->regOrID(true)
		};
	}
};

struct AddIInstruction:   BinaryIType<"+">   { using BinaryIType::BinaryIType; };
struct SubIInstruction:   BinaryIType<"-">   { using BinaryIType::BinaryIType; };
struct AndIInstruction:   BinaryIType<"&">   { using BinaryIType::BinaryIType; };
struct OrIInstruction:    BinaryIType<"|">   { using BinaryIType::BinaryIType; };
struct XorIInstruction:   BinaryIType<"^">   { using BinaryIType::BinaryIType; };
struct NandIInstruction:  BinaryIType<"~&">  { using BinaryIType::BinaryIType; };
struct NorIInstruction:   BinaryIType<"~|">  { using BinaryIType::BinaryIType; };
struct XnorIInstruction:  BinaryIType<"~^">  { using BinaryIType::BinaryIType; };
struct LandIInstruction:  BinaryIType<"&&">  { using BinaryIType::BinaryIType; };
struct LorIInstruction:   BinaryIType<"||">  { using BinaryIType::BinaryIType; };
struct LxorIInstruction:  BinaryIType<"^^">  { using BinaryIType::BinaryIType; };
struct LnandIInstruction: BinaryIType<"~&&"> { using BinaryIType::BinaryIType; };
struct LnorIInstruction:  BinaryIType<"~||"> { using BinaryIType::BinaryIType; };
struct LxnorIInstruction: BinaryIType<"~^^"> { using BinaryIType::BinaryIType; };
struct DivIInstruction:   BinaryIType<"/">   { using BinaryIType::BinaryIType; };
struct ModIInstruction:   BinaryIType<"%">   { using BinaryIType::BinaryIType; };
struct ShiftLeftLogicalIInstruction:     BinaryIType<"<<">  { using BinaryIType::BinaryIType; };
struct ShiftRightArithmeticIInstruction: BinaryIType<">>">  { using BinaryIType::BinaryIType; };
struct ShiftRightLogicalIInstruction:    BinaryIType<">>>"> { using BinaryIType::BinaryIType; };

template <fixstr::fixed_string O>
struct InverseBinaryRType: RType {
	using RType::RType;
	explicit operator std::vector<std::string>() const override {
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

struct SgRInstruction:  InverseBinaryRType<"<">  { using InverseBinaryRType::InverseBinaryRType; };
struct SgeRInstruction: InverseBinaryRType<"<="> { using InverseBinaryRType::InverseBinaryRType; };

template <fixstr::fixed_string O>
struct InverseUnsignedBinaryRType: RType {
	using RType::RType;
	explicit operator std::vector<std::string>() const override {
		return {
			rightSource->regOrID() + " " + std::string(O) + " " + leftSource->regOrID() + " -> " +
				destination->regOrID() + " /u"
		};
	}
	std::vector<std::string> colored() const override {
		return {
			rightSource->regOrID(true) + o(std::string(O)) + leftSource->regOrID(true) + o("->") +
				destination->regOrID(true) + " \e[2m/u\e[22m"
		};
	}
};

struct SguRInstruction: InverseUnsignedBinaryRType<"<">  {
	using InverseUnsignedBinaryRType::InverseUnsignedBinaryRType;
};

struct SgeuRInstruction: InverseUnsignedBinaryRType<"<="> {
	using InverseUnsignedBinaryRType::InverseUnsignedBinaryRType;
};

template <char O>
struct UnaryRType: RType {
	UnaryRType(VregPtr source_, VregPtr destination_): RType(source_, nullptr, destination_) {}
	explicit operator std::vector<std::string>() const override {
		return {std::string(1, O) + leftSource->regOrID() + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {
			"\e[2m" + std::string(1, O) + "\e[22m" + leftSource->regOrID(true) + " -> " + destination->regOrID(true)
		};
	}
};

struct NotRInstruction:  UnaryRType<'~'> { using UnaryRType::UnaryRType; };
struct LnotRInstruction: UnaryRType<'!'> { using UnaryRType::UnaryRType; };

template <fixstr::fixed_string O>
struct UnsignedBinaryRType: RType {
	using RType::RType;
	explicit operator std::vector<std::string>() const override {
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

struct DivuRInstruction: UnsignedBinaryRType<"/">  { using UnsignedBinaryRType::UnsignedBinaryRType; };
struct ModuRInstruction: UnsignedBinaryRType<"%">  { using UnsignedBinaryRType::UnsignedBinaryRType; };
struct SluRInstruction:  UnsignedBinaryRType<"<">  { using UnsignedBinaryRType::UnsignedBinaryRType; };
struct SleuRInstruction: UnsignedBinaryRType<"<="> { using UnsignedBinaryRType::UnsignedBinaryRType; };

template <fixstr::fixed_string O>
struct UnsignedBinaryIType: IType {
	using IType::IType;
	explicit operator std::vector<std::string>() const override {
		return {
			source->regOrID() + " " + std::string(O) + " " + stringify(imm) + " -> " + destination->regOrID() + " /u"
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
struct ModuIInstruction: UnsignedBinaryIType<"%"> { using UnsignedBinaryIType::UnsignedBinaryIType; };

/** Note that Why currently has no != instruction. */
struct ComparisonInstruction {
	Comparison comparison;
	bool isUnsigned;

	ComparisonInstruction(Comparison comparison_, bool is_unsigned): comparison(comparison_), isUnsigned(is_unsigned) {
		if (comparison_ == Comparison::Neq)
			throw std::invalid_argument("Comparison::Neq has no corresponding instruction");
	}

	[[nodiscard]] std::string oper() const {
		return comparison_map.at(comparison);
	}
};

/** $rs == (<=, <...) $rt -> $rd (/u) */
struct ComparisonRInstruction: RType, ComparisonInstruction {
	ComparisonRInstruction(const VregPtr &rs_, const VregPtr &rt_, const VregPtr &rd_, Comparison comparison_,
	bool is_unsigned):
		RType(rs_, rt_, rd_), ComparisonInstruction(comparison_, is_unsigned) {}
	explicit operator std::vector<std::string>() const override {
		return {
			leftSource->regOrID() + " " + oper() + " " + rightSource->regOrID() + " -> " + destination->regOrID()
		};
	}
	std::vector<std::string> colored() const override {
		return {
			leftSource->regOrID(true) + o(oper()) + rightSource->regOrID(true) + o("->") +
				destination->regOrID(true)
		};
	}
};

struct ComparisonIInstruction: IType, ComparisonInstruction {
	ComparisonIInstruction(const VregPtr &rs_, const VregPtr &rd_, const TypedImmediate &imm_, Comparison comparison_,
	bool is_unsigned):
		IType(rs_, rd_, imm_), ComparisonInstruction(comparison_, is_unsigned) {}
	explicit operator std::vector<std::string>() const override {
		return {
			source->regOrID() + " " + oper() + " " + stringify(imm) + " -> " + destination->regOrID()
		};
	}
	std::vector<std::string> colored() const override {
		return {
			source->regOrID(true) + o(oper()) + stringify(imm, true) + o("->") +
				destination->regOrID(true)
		};
	}
};

struct MemsetInstruction: RType {
	using RType::RType;
	explicit operator std::vector<std::string>() const override {
		return {
			"memset " + leftSource->regOrID() + " x " + rightSource->regOrID() + " -> " + destination->regOrID()
		};
	}
	std::vector<std::string> colored() const override {
		return {
			"\e[36mmemset\e[39m " + leftSource->regOrID(true) + " x " + rightSource->regOrID(true) + o("->") +
				destination->regOrID(true)
		};
	}
	std::vector<VregPtr> getRead() override { return {leftSource, rightSource, destination}; }
	std::vector<VregPtr> getWritten() override { return {}; }

	bool replaceRead(const VregPtr &from, const VregPtr &to) override {
		bool changed = false;
		if (leftSource == from) {
			leftSource = to;
			changed = true;
		}
		if (rightSource == from) {
			rightSource = to;
			changed = true;
		}
		if (destination == from) {
			destination = to;
			changed = true;
		}
		return changed;
	}

	bool canReplaceRead(const VregPtr &vreg) const override {
		return doesRead(vreg);
	}

	bool replaceWritten(const VregPtr &, const VregPtr &) override {
		return false;
	}

	bool canReplaceWritten(const VregPtr &) const override {
		return false;
	}

	bool doesRead(const VregPtr &vreg) const override {
		return vreg == leftSource || vreg == rightSource || vreg == destination;
	}

	bool doesWrite(const VregPtr &) const override {
		return false;
	}
};

struct CompareRInstruction: RType {
	CompareRInstruction(const VregPtr &rs_, const VregPtr &rt_): RType(rs_, rt_, nullptr) {}
	explicit operator std::vector<std::string>() const override {
		return {leftSource->regOrID() + " ~ " + rightSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {leftSource->regOrID(true) + " ~ " + rightSource->regOrID(true)};
	}
};

struct CompareIInstruction: IType {
	CompareIInstruction(const VregPtr &rs_, const TypedImmediate &imm_): IType(rs_, nullptr, imm_) {}
	explicit operator std::vector<std::string>() const override {
		return {source->regOrID() + " ~ " + stringify(imm)};
	}
	std::vector<std::string> colored() const override {
		return {source->regOrID(true) + " ~ " + stringify(imm, true)};
	}
};

class SelectInstruction: public RType {
	private:
		static const std::unordered_map<Condition, const char *> operMap;

	public:
		Condition condition;

		SelectInstruction(const VregPtr &rs_, const VregPtr &rt_, const VregPtr &rd_, Condition condition_):
			RType(rs_, rt_, rd_), condition(condition_) {}

		explicit operator std::vector<std::string>() const override {
			return {
				"[" + leftSource->regOrID() + " " + std::string(operMap.at(condition)) + " " + rightSource->regOrID() +
					"] -> " + destination->regOrID()
			};
		}

		std::vector<std::string> colored() const override {
			return {
				"\e[2m[\e[22m" + leftSource->regOrID(true) + " " + std::string(operMap.at(condition)) + " " +
					rightSource->regOrID(true) + "\e[2m] ->\e[22m " + destination->regOrID(true)
			};
		}
};

template <fixstr::fixed_string O>
struct InverseBinaryIType: IType {
	using IType::IType;
	explicit operator std::vector<std::string>() const override {
		return {stringify(imm) + " " + std::string(O) + " " + source->regOrID() + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {
			stringify(imm, true) + o(std::string(O)) + source->regOrID(true) + o("->") + destination->regOrID(true)
		};
	}
};

struct DiviIInstruction: InverseBinaryIType<"/"> { using InverseBinaryIType::InverseBinaryIType; };
struct ShiftLeftLogicalInverseIInstruction: InverseBinaryIType<"<<"> { using InverseBinaryIType::InverseBinaryIType; };
struct ShiftRightLogicalInverseIInstruction: InverseBinaryIType<">>>"> {
	using InverseBinaryIType::InverseBinaryIType; };
struct ShiftRightArithmeticInverseIInstruction: InverseBinaryIType<">>"> {
	using InverseBinaryIType::InverseBinaryIType; };

template <fixstr::fixed_string O>
struct InverseUnsignedBinaryIType: IType {
	using IType::IType;
	explicit operator std::vector<std::string>() const override {
		return {
			stringify(imm) + " " + std::string(O) + " " + source->regOrID() + " -> " + destination->regOrID() + " /u"
		};
	}
	std::vector<std::string> colored() const override {
		return {
			stringify(imm, true) + o(std::string(O)) + source->regOrID(true) + o("->") + destination->regOrID(true)
				+ " \e[2m/u\e[22m"
		};
	}
};

struct DivuiIInstruction: InverseUnsignedBinaryIType<"/"> {
	using InverseUnsignedBinaryIType::InverseUnsignedBinaryIType; };

struct Nop: WhyInstruction {
	Nop() = default;
	explicit operator std::vector<std::string>() const override { return {"<>"}; }
	std::vector<std::string> colored() const override { return {"<>"}; }
};

template <fixstr::fixed_string N>
struct SimpleIType: IType {
	explicit SimpleIType(const TypedImmediate &imm_): IType(nullptr, nullptr, imm_) {}
	explicit operator std::vector<std::string>() const override {
		return {"%" + std::string(N) + " " + stringify(imm)};
	}
	std::vector<std::string> colored() const override {
		return {"\e[36m%" + std::string(N) + "\e[39m " + stringify(imm, true)};
	}
};

struct IntIInstruction:   SimpleIType<"int">   { using SimpleIType::SimpleIType; };
struct RitIInstruction:   SimpleIType<"rit">   { using SimpleIType::SimpleIType; };
struct TimeIInstruction:  SimpleIType<"time">  { using SimpleIType::SimpleIType; };
struct RingIInstruction:  SimpleIType<"ring">  { using SimpleIType::SimpleIType; };
struct SetptIInstruction: SimpleIType<"setpt"> { using SimpleIType::SimpleIType; };

template <fixstr::fixed_string N>
struct SimpleRType: RType {
	explicit SimpleRType(const VregPtr &rs_): RType(rs_, nullptr, nullptr) {}
	explicit operator std::vector<std::string>() const override {
		return {"%" + std::string(N) + " " + leftSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[36m%" + std::string(N) + "\e[39m " + leftSource->regOrID(true)};
	}
};

struct IntRInstruction:   SimpleRType<"int">   { using SimpleRType::SimpleRType; };
struct RitRInstruction:   SimpleRType<"rit">   { using SimpleRType::SimpleRType; };
struct TimeRInstruction:  SimpleRType<"time">  { using SimpleRType::SimpleRType; };
struct RingRInstruction:  SimpleRType<"ring">  { using SimpleRType::SimpleRType; };

template <fixstr::fixed_string N>
struct SaveRInstruction: RType {
	explicit SaveRInstruction(const VregPtr &rd_): RType(nullptr, nullptr, rd_) {}
	explicit operator std::vector<std::string>() const override {
		return {"%" + std::string(N) + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"\e[36m%" + std::string(N) + "\e[39m" + o("->") + destination->regOrID(true)};
	}
};

struct SvtimeRInstruction: SaveRInstruction<"time"> { using SaveRInstruction::SaveRInstruction; };
struct SvringRInstruction: SaveRInstruction<"ring"> { using SaveRInstruction::SaveRInstruction; };
struct SvpgRInstruction:   SaveRInstruction<"page"> { using SaveRInstruction::SaveRInstruction; };

struct PrintRInstruction: RType {
	PrintType type;

	PrintRInstruction(const VregPtr &rs_, PrintType type_): RType(rs_, nullptr, nullptr), type(type_) {}

	explicit operator std::vector<std::string>() const override {
		return {"<" + typeName() + " " + leftSource->regOrID() + ">"};
	}

	std::vector<std::string> colored() const override {
		return {"<\e[36m" + typeName() + "\e[39m " + leftSource->regOrID(true) + ">"};
	}

	private:
		std::string typeName() const {
			switch (type) {
				case PrintType::Bin:  return "prb";
				case PrintType::Dec:  return "prd";
				case PrintType::Hex:  return "prx";
				case PrintType::Char: return "prc";
				case PrintType::Full: return "print";
				default: return "???";
			}
		}
};

template <fixstr::fixed_string N>
struct TrivialExternalInstruction: RType {
	TrivialExternalInstruction(): RType(nullptr, nullptr, nullptr) {}
	explicit operator std::vector<std::string>() const override { return {"<" + std::string(N) + ">"};   }
	std::vector<std::string> colored() const override { return {"<\e[36m" + std::string(N) + "\e[39m>"}; }
};

struct HaltRInstruction: TrivialExternalInstruction<"halt"> {
	using TrivialExternalInstruction::TrivialExternalInstruction; };

struct RestRInstruction: TrivialExternalInstruction<"rest"> {
	using TrivialExternalInstruction::TrivialExternalInstruction; };

struct SleepRInstruction: RType {
	explicit SleepRInstruction(const VregPtr &rs_): RType(rs_, nullptr, nullptr) {}
	explicit operator std::vector<std::string>() const override {
		return {"<sleep " + leftSource->regOrID() + ">"};
	}
	std::vector<std::string> colored() const override {
		return {"<\e[36msleep\e[39m " + leftSource->regOrID(true) + ">"};
	}
};

struct PageRInstruction: RType {
	bool on;
	explicit PageRInstruction(bool on_): RType(nullptr, nullptr, nullptr), on(on_) {}
	explicit operator std::vector<std::string>() const override {
		return {"%page " + std::string(on? "on" : "off")};
	}
	std::vector<std::string> colored() const override {
		return {"\e[36m%page\e[39m " + std::string(on? "on" : "off")};
	}
};

struct QueryRInstruction: RType {
	QueryType type;
	QueryRInstruction(const VregPtr &rd_, QueryType type_): RType(nullptr, nullptr, rd_), type(type_) {}
	explicit operator std::vector<std::string>() const override {
		return {"? " + query_map.at(type) + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"? \e[36m" + query_map.at(type) + "\e[39m \e[2m->\e[22m " + destination->regOrID(true)};
	}
};

struct PrintPseudoinstruction: IType {
	std::string text;
	bool useText = false;

	explicit PrintPseudoinstruction(TypedImmediate imm_):
		IType(nullptr, nullptr, std::move(imm_)) {}

	explicit PrintPseudoinstruction(const std::string &text_):
		IType(nullptr, nullptr, OperandType::ULONG, 0), text(text_), useText(true) {}

	explicit operator std::vector<std::string>() const override {
		return {(useText? "<p \"" + Util::escape(text) + "\"" : "<prc " + charify(imm)) + ">"};
	}
	std::vector<std::string> colored() const override {
		return {"<\e[36mp" + (useText? "\e[39m \"" + Util::escape(text) + "\"" : "rc\e[39m " + charify(imm)) + ">"};
	}
};

struct IOInstruction: RType {
	std::string type;
	explicit IOInstruction(const std::string &type_): RType(nullptr, nullptr, nullptr), type(type_) {}
	explicit operator std::vector<std::string>() const override {
		return {"<io" + (type.empty()? "" : " " + type) + ">"};
	}
	std::vector<std::string> colored() const override {
		return {"<\e[36mio\e[39m" + (type.empty()? "" : " " + type) + ">"};
	}
};

struct InterruptsInstruction: RType {
	bool enable;
	explicit InterruptsInstruction(bool enable_): RType(nullptr, nullptr, nullptr), enable(enable_) {}
	explicit operator std::vector<std::string>() const override {
		return {enable? "%ei" : "%di"};
	}
	std::vector<std::string> colored() const override {
		return {"\e[34m" + std::string(enable? "%ei" : "%di") + "\e[39m"};
	}
};

struct TranslateAddressRInstruction: RType {
	TranslateAddressRInstruction(const VregPtr &rs_, const VregPtr &rd_): RType(rs_, nullptr, rd_) {}
	explicit operator std::vector<std::string>() const override {
		return {"translate " + leftSource->regOrID() + " -> " + destination->regOrID()};
	}
	std::vector<std::string> colored() const override {
		return {"translate " + leftSource->regOrID(true) + o("->") + destination->regOrID(true)};
	}
};

struct PageStackInstruction: RType {
	bool isPush;
	PageStackInstruction(bool is_push, const VregPtr &rs_): RType(rs_, nullptr, nullptr), isPush(is_push) {}
	explicit operator std::vector<std::string>() const override {
		if (!leftSource)
			return {std::string(isPush? "[" : "]") + " %page"};
		return {": " + std::string(isPush? "[" : "]") + " %page " + leftSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		if (!leftSource)
			return {"\e[2m" + std::string(isPush? "[" : "]") + "\e[22m \e[34m%page\e[39m"};
		return {"\e[2m: " + std::string(isPush? "[" : "]") + "\e[22m \e[34m%page\e[39m " + leftSource->regOrID(true)};
	}
};

struct SetptRInstruction: RType {
	SetptRInstruction(const VregPtr &rs_, const VregPtr &rt_): RType(rs_, rt_, nullptr) {}
	explicit operator std::vector<std::string>() const override {
		if (!rightSource)
			return {"%setpt " + leftSource->regOrID()};
		return {": %setpt " + leftSource->regOrID() + " " + rightSource->regOrID()};
	}
	std::vector<std::string> colored() const override {
		if (!rightSource)
			return {"\e[36m%setpt\e[39m " + leftSource->regOrID(true)};
		return {"\e[2m:\e[22m \e[36m%setpt\e[39m " + leftSource->regOrID(true) + " " + rightSource->regOrID(true)};
	}
};

struct CallPushPlaceholder: WhyInstruction {
	bool enableDebug() const override { return false; }
	explicit operator std::vector<std::string>() const override { return {"//! Untranslated CallPushPlaceholder"};   }
	std::vector<std::string> colored() const override { return {"\e[31m//! Untranslated CallPushPlaceholder\e[39m"}; }
};

struct CallPopPlaceholder: WhyInstruction {
	bool enableDebug() const override { return false; }
	explicit operator std::vector<std::string>() const override { return {"//! Untranslated CallPopPlaceholder"};   }
	std::vector<std::string> colored() const override { return {"\e[31m//! Untranslated CallPopPlaceholder\e[39m"}; }
};
