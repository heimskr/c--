#include "WhyInstructions.h"

const std::unordered_map<Condition, const char *> SelectInstruction::operMap {
	{Condition::Zero,     "="},
	{Condition::Positive, ">"},
	{Condition::Negative, "<"},
	{Condition::Nonzero,  "!="},
};
