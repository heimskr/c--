#pragma once

#include <memory>
#include <ostream>
#include <string>

#include "Util.h"

struct BasicBlock;

// Base class for both c-- instructions and Why instructions.
struct Instruction {
	std::weak_ptr<BasicBlock> parent;
	int index = -1;
	virtual ~Instruction() {}
	virtual operator std::vector<std::string>() const { return {"???"}; }
	virtual std::string joined(bool is_colored = true, const std::string &delimiter = "\n\t") const {
		return Util::join(is_colored? colored() : std::vector<std::string>(*this), delimiter);
	}
	virtual std::vector<std::string> colored() const { return std::vector<std::string>(*this); }
	Instruction & setParent(std::weak_ptr<BasicBlock> parent_) { parent = parent_; return *this; }
	Instruction & setIndex(int index_) { index = index_; return *this; }
	std::string singleLine() const;
};

using InstructionPtr = std::shared_ptr<Instruction>;

std::ostream & operator<<(std::ostream &, const Instruction &);
