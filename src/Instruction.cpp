#include "Instruction.h"

std::ostream & operator<<(std::ostream &os, const Instruction &instruction) {
	return os << instruction.singleLine();
}

std::string Instruction::singleLine() const {
	return joined(true, "; ");
}
