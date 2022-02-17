#pragma once

#include <list>
#include <memory>
#include <set>

#include "Makeable.h"
#include "WeakSet.h"

struct WhyInstruction;

struct BasicBlock: Makeable<BasicBlock> {
	std::string label;
	std::list<std::shared_ptr<WhyInstruction>> instructions;
	WeakSet<BasicBlock> predecessors, successors;
	BasicBlock(const std::string &label_): label(label_) {}
	BasicBlock & operator+=(std::shared_ptr<WhyInstruction> instruction) {
		instructions.push_back(instruction);
		return *this;
	}
	operator bool() const { return !instructions.empty(); }
};

using BasicBlockPtr = std::shared_ptr<BasicBlock>;
