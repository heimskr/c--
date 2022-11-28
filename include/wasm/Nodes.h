#pragma once

#include <memory>
#include <unordered_map>

#include "ASTNode.h"
#include "Enums.h"
#include "Immediate.h"

enum class WASMNodeType {
	Immediate, RType, IType, Copy, Load, Store, Set, Li, Si, Lni, Cmp, Cmpi, Sel, J, Jc, Jr, Jrc, Mv,
	SizedStack, MultR, MultI, DiviI, Lui, Stack, Nop, IntI, RitI, TimeI, TimeR, RingI, RingR, Print, Halt, SleepR,
	Page, SetptI, Label, SetptR, Svpg, Query, PseudoPrint, Rest, IO, Interrupts, InverseShift, Sext, PageStack,
	TranslateAddress, Svring, Svtime,
};

class Function;
struct Variable;
struct VirtualRegister;
struct WhyInstruction;

using VarMap = std::unordered_map<std::string, std::shared_ptr<VirtualRegister>>;

struct WASMBaseNode: ASTNode {
	explicit WASMBaseNode(int sym);
	virtual WASMNodeType nodeType() const = 0;
	virtual explicit operator std::string() const = 0;
};

struct WASMInstructionNode: WASMBaseNode {
	using WASMBaseNode::WASMBaseNode;

	static std::shared_ptr<VirtualRegister> convertVariable(Function &, VarMap &, const std::string *);
	virtual std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) { return nullptr; }
};

struct WASMImmediateNode: WASMBaseNode {
	TypedImmediate imm;

	explicit WASMImmediateNode(ASTNode *);
	WASMNodeType nodeType() const override { return WASMNodeType::Immediate; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
};

struct WASMLabelNode: WASMInstructionNode { // Not technically an instruction, but still.
	const std::string *label;

	explicit WASMLabelNode(ASTNode *);
	explicit WASMLabelNode(const std::string *);
	WASMNodeType nodeType() const override { return WASMNodeType::Label; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct RNode: WASMInstructionNode {
	const std::string *rs, *oper, *rt, *rd;
	int operToken;
	
	RNode(ASTNode *rs_, ASTNode *oper_, ASTNode *rt_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::RType; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct INode: WASMInstructionNode {
	const std::string *rs, *oper, *rd;
	int operToken;
	TypedImmediate imm;

	INode(ASTNode *rs_, ASTNode *oper_, ASTNode *imm, ASTNode *rd_);
	INode(const std::string *rs_, const std::string *oper_, TypedImmediate imm_, const std::string *rd_,
	      int oper_token);
	WASMNodeType nodeType() const override { return WASMNodeType::IType; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMMemoryNode: WASMInstructionNode {
	const std::string *rs;
	const std::string *rd;

	WASMMemoryNode(int sym, ASTNode *rs_, ASTNode *rd_);
};

struct WASMCopyNode: WASMMemoryNode {
	WASMCopyNode(ASTNode *rs_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Copy; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMLoadNode: WASMMemoryNode {
	WASMLoadNode(ASTNode *rs_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Load; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMStoreNode: WASMMemoryNode {
	WASMStoreNode(ASTNode *rs_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Store; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSetNode: WASMInstructionNode {
	const std::string *rd;
	TypedImmediate imm;

	WASMSetNode(ASTNode *imm_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Set; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMLiNode: WASMInstructionNode {
	const std::string *rd;
	TypedImmediate imm;

	WASMLiNode(ASTNode *imm_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Li; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSiNode: WASMInstructionNode {
	const std::string *rs;
	TypedImmediate imm;

	WASMSiNode(ASTNode *rs_, ASTNode *imm_);
	WASMNodeType nodeType() const override { return WASMNodeType::Si; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMLniNode: WASMLiNode {
	WASMLniNode(ASTNode *imm_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Lni; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMMidMemoryNode: WASMInstructionNode {
	const std::string *rs, *rd;

	WASMMidMemoryNode(int sym, ASTNode *rs_, ASTNode *rd_);
};

struct WASMCmpNode: WASMInstructionNode {
	const std::string *rs, *rt;

	WASMCmpNode(ASTNode *rs_, ASTNode *rt_);
	WASMNodeType nodeType() const override { return WASMNodeType::Cmp; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMCmpiNode: WASMInstructionNode {
	const std::string *rs;
	TypedImmediate imm;

	WASMCmpiNode(ASTNode *rs_, ASTNode *imm_);
	WASMNodeType nodeType() const override { return WASMNodeType::Cmpi; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSelNode: WASMInstructionNode {
	const std::string *rs, *rt, *rd;
	Condition condition = Condition::Zero;

	WASMSelNode(ASTNode *rs_, ASTNode *oper_, ASTNode *rt_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Sel; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMJNode: WASMInstructionNode {
	TypedImmediate addr;
	Condition condition;
	bool link;

	WASMJNode(ASTNode *cond, ASTNode *colons, ASTNode *addr_);
	WASMNodeType nodeType() const override { return WASMNodeType::J; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMJcNode: WASMInstructionNode {
	bool link;
	TypedImmediate addr;
	const std::string *rs;

	WASMJcNode(WASMJNode *, ASTNode *rs_);
	WASMNodeType nodeType() const override { return WASMNodeType::Jc; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

// Used for both jr and jrl.
struct WASMJrNode: WASMInstructionNode {
	Condition condition;
	bool link;
	const std::string *rd;

	WASMJrNode(ASTNode *cond, ASTNode *colons, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Jr; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

// Used for both jrc and jrlc.
struct WASMJrcNode: WASMInstructionNode {
	bool link;
	const std::string *rs, *rd;

	WASMJrcNode(WASMJrNode *, ASTNode *rs_);
	WASMNodeType nodeType() const override { return WASMNodeType::Jrc; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMMultRNode: WASMInstructionNode {
	const std::string *rs, *rt;

	WASMMultRNode(ASTNode *rs_, ASTNode *rt_);
	WASMNodeType nodeType() const override { return WASMNodeType::MultR; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMMultINode: WASMInstructionNode {
	const std::string *rs;
	TypedImmediate imm;

	WASMMultINode(ASTNode *rs_, ASTNode *imm_);
	WASMNodeType nodeType() const override { return WASMNodeType::MultI; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMDiviINode: WASMInstructionNode {
	const std::string *rs, *rd;
	TypedImmediate imm;

	WASMDiviINode(ASTNode *imm_, ASTNode *rs_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::DiviI; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMLuiNode: WASMInstructionNode {
	const std::string *rd;
	TypedImmediate imm;

	WASMLuiNode(ASTNode *imm_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Lui; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMStackNode: WASMInstructionNode {
	const std::string *reg;
	bool isPush;

	WASMStackNode(ASTNode *reg_, bool is_push);
	WASMNodeType nodeType() const override { return WASMNodeType::Stack; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMNopNode: WASMInstructionNode {
	WASMNopNode();
	WASMNodeType nodeType() const override { return WASMNodeType::Nop; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMIntINode: WASMInstructionNode {
	TypedImmediate imm;

	explicit WASMIntINode(ASTNode *imm_);
	WASMNodeType nodeType() const override { return WASMNodeType::IntI; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMRitINode: WASMInstructionNode {
	TypedImmediate imm;

	explicit WASMRitINode(ASTNode *imm_);
	WASMNodeType nodeType() const override { return WASMNodeType::RitI; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMTimeINode: WASMInstructionNode {
	TypedImmediate imm;

	explicit WASMTimeINode(ASTNode *imm_);
	WASMNodeType nodeType() const override { return WASMNodeType::TimeI; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMTimeRNode: WASMInstructionNode {
	const std::string *rs;

	explicit WASMTimeRNode(ASTNode *rs_);
	WASMNodeType nodeType() const override { return WASMNodeType::TimeR; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSvtimeNode: WASMInstructionNode {
	const std::string *rd;

	explicit WASMSvtimeNode(ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Svtime; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMRingINode: WASMInstructionNode {
	TypedImmediate imm;

	explicit WASMRingINode(ASTNode *imm_);
	WASMNodeType nodeType() const override { return WASMNodeType::RingI; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMRingRNode: WASMInstructionNode {
	const std::string *rs;

	explicit WASMRingRNode(ASTNode *rs_);
	WASMNodeType nodeType() const override { return WASMNodeType::RingR; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSvringNode: WASMInstructionNode {
	const std::string *rd;

	explicit WASMSvringNode(ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Svring; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMPrintNode: WASMInstructionNode {
	const std::string *rs;
	PrintType type;

	WASMPrintNode(ASTNode *rs_, ASTNode *type_);
	WASMNodeType nodeType() const override { return WASMNodeType::Print; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMHaltNode: WASMInstructionNode {
	WASMHaltNode();
	WASMNodeType nodeType() const override { return WASMNodeType::Halt; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSleepRNode: WASMInstructionNode {
	const std::string *rs;

	explicit WASMSleepRNode(ASTNode *rs_);
	WASMNodeType nodeType() const override { return WASMNodeType::SleepR; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMPageNode: WASMInstructionNode {
	bool on;

	explicit WASMPageNode(bool on_);
	WASMNodeType nodeType() const override { return WASMNodeType::Page; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSetptINode: WASMInstructionNode {
	TypedImmediate imm;

	explicit WASMSetptINode(ASTNode *imm_);
	WASMNodeType nodeType() const override { return WASMNodeType::SetptI; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSetptRNode: WASMInstructionNode {
	const std::string *rs, *rt;

	explicit WASMSetptRNode(ASTNode *rs_, ASTNode *rt_ = nullptr);
	explicit WASMSetptRNode(const std::string *rs_, const std::string *rt_ = nullptr);
	WASMNodeType nodeType() const override { return WASMNodeType::SetptR; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMMvNode: WASMInstructionNode {
	const std::string *rs, *rd;

	WASMMvNode(ASTNode *rs_, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Mv; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMSvpgNode: WASMInstructionNode {
	const std::string *rd;

	explicit WASMSvpgNode(ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Svpg; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMQueryNode: WASMInstructionNode {
	QueryType type;
	const std::string *rd;

	WASMQueryNode(QueryType, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::Query; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMPseudoPrintNode: WASMInstructionNode {
	TypedImmediate imm;
	const std::string *printText = nullptr;

	explicit WASMPseudoPrintNode(ASTNode *imm_);
	explicit WASMPseudoPrintNode(const std::string *print_text);
	WASMNodeType nodeType() const override { return WASMNodeType::PseudoPrint; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMRestNode: WASMInstructionNode {
	WASMRestNode();
	WASMNodeType nodeType() const override { return WASMNodeType::Rest; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMIONode: WASMInstructionNode {
	const std::string *ident = nullptr;

	explicit WASMIONode(const std::string *ident_);
	WASMNodeType nodeType() const override { return WASMNodeType::IO; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMInterruptsNode: WASMInstructionNode {
	const bool enable;

	explicit WASMInterruptsNode(bool enable_);
	WASMNodeType nodeType() const override { return WASMNodeType::Interrupts; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMInverseShiftNode: WASMInstructionNode {
	const std::string *rs, *oper, *rd;
	int operToken;
	TypedImmediate imm;

	WASMInverseShiftNode(ASTNode *rs_, ASTNode *oper_, ASTNode *imm, ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::InverseShift; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMTransNode: WASMInstructionNode {
	const std::string *rs, *rd;

	WASMTransNode(const ASTNode *rs_, const ASTNode *rd_);
	WASMNodeType nodeType() const override { return WASMNodeType::TranslateAddress; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};

struct WASMPageStackNode: WASMInstructionNode {
	bool isPush;
	const std::string *rs;
	WASMPageStackNode(bool is_push, const ASTNode *rs_);
	explicit WASMPageStackNode(bool is_push, const std::string *rs_ = nullptr);
	WASMNodeType nodeType() const override { return WASMNodeType::PageStack; }
	std::string debugExtra() const override;
	explicit operator std::string() const override;
	std::unique_ptr<WhyInstruction> convert(Function &, VarMap &) override;
};
