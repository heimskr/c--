#pragma once

#include <string>

// Base class for both c-- instructions and Why instructions.
struct Instruction {
	virtual operator std::string() const { return "???"; }
	virtual std::string  colored() const { return std::string(*this); }
};
