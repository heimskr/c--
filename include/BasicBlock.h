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

	BasicBlock(Function &function_, std::string label_): function(function_), label(std::move(label_)) {}

	BasicBlock & operator+=(const std::shared_ptr<WhyInstruction> &instruction) {
		instructions.push_back(instruction);
		return *this;
	}

	explicit operator bool() const { return !instructions.empty(); }

	/** Returns a set of all variables (excluding globals and register-allocated/precolored) referenced in the block. */
	[[nodiscard]] std::set<std::shared_ptr<VirtualRegister>> gatherVariables() const;

	/** Returns the number of unique variables (excluding globals and register-allocated/precolored) referenced in the
	 *  block. */
	[[nodiscard]] size_t countVariables() const;

	void cacheReadWritten();
} __attribute__((packed, aligned(128)));

using BasicBlockPtr = std::shared_ptr<BasicBlock>;
