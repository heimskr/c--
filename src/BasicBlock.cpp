#include "BasicBlock.h"
#include "Global.h"
#include "Variable.h"
#include "WhyInstructions.h"

std::set<VregPtr> BasicBlock::gatherVariables() const {
	std::set<VregPtr> out;

	for (const auto &instruction: instructions) {
		for (const auto &read: instruction->getRead())
			if (!read->is<Global>() && read->reg < 0)
				out.insert(read);
		for (const auto &written: instruction->getWritten())
			if (!written->is<Global>() && written->reg < 0)
				out.insert(written);
	}

	return out;
}

size_t BasicBlock::countVariables() const {
	return gatherVariables().size();
}
