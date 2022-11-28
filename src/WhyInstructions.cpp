#include "WhyInstructions.h"
#include "Type.h"

const std::unordered_map<Condition, const char *> SelectInstruction::operMap {
	{Condition::Zero,     "="},
	{Condition::Positive, ">"},
	{Condition::Negative, "<"},
	{Condition::Nonzero,  "!="},
};

MultIInstruction::operator std::vector<std::string>() const {
	return {
		source->regOrID() + " * " + stringify(imm),
		"$lo" + std::string(OperandType(*source->getType())) + " -> " + destination->regOrID()
	};
}

std::vector<std::string> MultIInstruction::colored() const {
	return {
		source->regOrID(true) + o("*") + stringify(imm, true),
		Why::coloredRegister(Why::loOffset) + std::string(OperandType(*source->getType())) + o("->") +
			destination->regOrID(true)
	};
}

MultRInstruction::operator std::vector<std::string>() const {
	return {
		leftSource->regOrID() + " * " + rightSource->regOrID(),
		"$lo" + std::string(OperandType(*leftSource->getType())) + " -> " + destination->regOrID()
	};
}

std::vector<std::string> MultRInstruction::colored() const {
	return {
		leftSource->regOrID(true) + o("*") + rightSource->regOrID(true),
		Why::coloredRegister(Why::loOffset) + std::string(OperandType(*leftSource->getType())) + o("->") +
			destination->regOrID(true)
	};
}

OperandType SextInstruction::getType(const VregPtr &source, int width) {
	OperandType out = OperandType(*source->getType());
	assert(out.pointerLevel == 0);
	switch (width) {
		case 8:  out.primitive = Primitive::Char;  break;
		case 16: out.primitive = Primitive::Short; break;
		case 32: out.primitive = Primitive::Int;   break;
		case 64: out.primitive = Primitive::Long;  break;
		default: throw std::invalid_argument("Invalid sext width: " + std::to_string(width));
	}
	return out;
}
