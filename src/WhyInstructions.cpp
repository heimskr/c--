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
