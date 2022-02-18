#pragma once

#include <memory>
#include <ostream>
#include <string>

#include "Checkable.h"
#include "Makeable.h"
#include "WeakSet.h"

class Function;
struct BasicBlock;
struct Scope;
struct Type;
struct WhyInstruction;

struct VirtualRegister: Checkable, std::enable_shared_from_this<VirtualRegister> {
	Function *function = nullptr;
	int id;
	int reg = -1;
	bool precolored = false;

	VirtualRegister(Function &);
	VirtualRegister(int id_);
	std::shared_ptr<VirtualRegister> init();

	std::string regOrID(bool colored = false) const;
	bool special() const;

	WeakSet<BasicBlock> readingBlocks, writingBlocks;
	WeakSet<WhyInstruction> readers, writers;

	virtual operator std::string() const { return regOrID(true); }
};

struct Variable: VirtualRegister, Makeable<Variable> {
	std::string name;
	std::shared_ptr<Type> type;
	std::weak_ptr<Scope> scope; // ???

	Variable(const std::string &name_, std::shared_ptr<Type>, Function &);
	Variable(const std::string &name_, std::shared_ptr<Type>);

	virtual ~Variable() {}

	size_t getSize() const;
	operator std::string() const override;
};

using VregPtr = std::shared_ptr<VirtualRegister>;
using VariablePtr = std::shared_ptr<Variable>;

std::ostream & operator<<(std::ostream &, const VirtualRegister &);
