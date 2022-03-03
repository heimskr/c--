#pragma once

#include <memory>
#include <ostream>
#include <string>

#include "DebugData.h"
#include "Util.h"

struct BasicBlock;
struct Expr;

struct Instruction {
	DebugData debug;
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
	Instruction & setDebug(const DebugData &debug_) { debug = debug_; return *this; }
	Instruction & setDebug(const Expr &);
};

using InstructionPtr = std::shared_ptr<Instruction>;

std::ostream & operator<<(std::ostream &, const Instruction &);
