#pragma once

#include <memory>
#include <ostream>
#include <string>

#include "DebugData.h"
#include "Util.h"

struct BasicBlock;
struct Expr;

struct Instruction {
	public:
		DebugData debug;
		std::weak_ptr<BasicBlock> parent;
		int index = -1;
		Instruction(const Instruction &) = delete;
		Instruction(Instruction &&) = delete;
		Instruction & operator=(const Instruction &) = delete;
		Instruction & operator=(Instruction &&) = delete;
		virtual ~Instruction() = default;
		virtual explicit operator std::vector<std::string>() const { return {"???"}; }
		[[nodiscard]] virtual std::string joined(bool is_colored, const std::string &delimiter) const {
			return Util::join(is_colored? colored() : std::vector<std::string>(*this), delimiter);
		}
		[[nodiscard]] virtual std::vector<std::string> colored() const { return std::vector<std::string>(*this); }
		Instruction & setParent(const std::weak_ptr<BasicBlock> &parent_) { parent = parent_; return *this; }
		Instruction & setIndex(int index_) { index = index_; return *this; }
		[[nodiscard]] std::string singleLine() const;
		Instruction & setDebug(const DebugData &debug_) { debug = debug_; return *this; }
		Instruction & setDebug(const Expr &);

	protected:
		Instruction() = default;
};

using InstructionPtr = std::shared_ptr<Instruction>;

std::ostream & operator<<(std::ostream &, const Instruction &);
