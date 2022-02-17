#pragma once

#include <string>

#include "Util.h"

// Base class for both c-- instructions and Why instructions.
struct Instruction {
	virtual ~Instruction() {}
	virtual operator std::vector<std::string>() const { return {"???"}; }
	virtual std::string joined(const std::string &delimiter = "\n\t") const {
		return Util::join(std::vector<std::string>(*this), delimiter);
	}
};
