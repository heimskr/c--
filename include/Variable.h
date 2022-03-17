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

	protected:
		std::shared_ptr<Type> type;

	public:
		Function *function = nullptr;
		int id;
		bool precolored = false;

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
		std::shared_ptr<Type> getType() { return type; }
		std::shared_ptr<const Type> getType() const { return type; }
		virtual VirtualRegister * setType(const Type &);
};

struct Variable: VirtualRegister, Makeable<Variable> {
	std::string name;

	Variable(const std::string &name_, std::shared_ptr<Type>, Function &);
	Variable(const std::string &name_, std::shared_ptr<Type>);

	operator std::string() const override;

	VirtualRegister * setType(const Type &type_) override;
};

using VregPtr = std::shared_ptr<VirtualRegister>;
using VariablePtr = std::shared_ptr<Variable>;

struct VregLess {
	bool operator()(const VregPtr &left, const VregPtr &right) const {
		if (left && !right)
			return true;
		if (!left)
			return false;
		return left->id < right->id;
	}
};

using VregSet = std::set<VregPtr, VregLess>;

std::ostream & operator<<(std::ostream &, const VirtualRegister &);
