#include "Expr.h"
#include "Instruction.h"

std::ostream & operator<<(std::ostream &os, const Instruction &instruction) {
	return os << instruction.singleLine();
}

std::string Instruction::singleLine() const {
	return joined(true, "; ");
}

Instruction & Instruction::setDebug(const Expr &expr) {
	return setDebug(expr.debug);
}
