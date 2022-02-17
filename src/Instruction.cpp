#include "Instruction.h"

std::ostream & operator<<(std::ostream &os, const Instruction &instruction) {
	return os << instruction.joined(true, "; ");
}
