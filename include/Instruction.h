#pragma once

#include <memory>
#include <string>

#include "Util.h"

// Base class for both c-- instructions and Why instructions.
struct Instruction {
	virtual ~Instruction() {}
	virtual operator std::vector<std::string>() const { return {"???"}; }
	virtual std::string joined(bool is_colored = true, const std::string &delimiter = "\n\t") const {
		return Util::join(is_colored? colored() : std::vector<std::string>(*this), delimiter);
	}
	virtual std::vector<std::string> colored() const { return std::vector<std::string>(*this); }
};

using InstructionPtr = std::shared_ptr<Instruction>;
