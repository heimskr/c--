#pragma once

#include <list>
#include <memory>
#include <set>
#include <string>

#include "Makeable.h"
#include "WeakSet.h"

class Function;
class Node;
struct VirtualRegister;
struct WhyInstruction;

struct BasicBlock: Makeable<BasicBlock> {
	Function &function;
	std::string label;
	std::list<std::shared_ptr<WhyInstruction>> instructions;
	WeakSet<BasicBlock> predecessors, successors;
	std::set<std::shared_ptr<VirtualRegister>> liveIn, liveOut, readCache, writtenCache;
	Node *node = nullptr;
	int index = -1;

	BasicBlock(Function &function_, const std::string &label_): function(function_), label(label_) {}

	BasicBlock & operator+=(std::shared_ptr<WhyInstruction> instruction) {
		instructions.push_back(instruction);
		return *this;
	}

	operator bool() const { return !instructions.empty(); }

	/** Returns a set of all variables (excluding globals and register-allocated/precolored) referenced in the block. */
	std::set<std::shared_ptr<VirtualRegister>> gatherVariables() const;

	/** Returns the number of unique variables (excluding globals and register-allocated/precolored) referenced in the
	 *  block. */
	size_t countVariables() const;

	void cacheReadWritten();
};

using BasicBlockPtr = std::shared_ptr<BasicBlock>;
