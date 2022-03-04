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
	private:
		int reg = -1;

	public:
		Function *function = nullptr;
		int id;
		bool precolored = false;
		std::shared_ptr<Type> type;

		VirtualRegister(Function &, std::shared_ptr<Type> = nullptr);
		VirtualRegister(int id_, std::shared_ptr<Type> = nullptr);
		std::shared_ptr<VirtualRegister> init();

		virtual ~VirtualRegister() {}

		std::string regOrID(bool colored = false) const;
		bool special() const;

		WeakSet<BasicBlock> readingBlocks, writingBlocks;
		WeakSet<WhyInstruction> readers, writers;

		size_t getSize() const;
		virtual operator std::string() const { return regOrID(true); }
		VirtualRegister & setReg(int, bool bypass = false);
		int getReg() const { return reg; }
};

struct Variable: VirtualRegister, Makeable<Variable> {
	std::string name;

	Variable(const std::string &name_, std::shared_ptr<Type>, Function &);
	Variable(const std::string &name_, std::shared_ptr<Type>);

	operator std::string() const override;
};

using VregPtr = std::shared_ptr<VirtualRegister>;
using VariablePtr = std::shared_ptr<Variable>;

std::ostream & operator<<(std::ostream &, const VirtualRegister &);
