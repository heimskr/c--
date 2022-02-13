#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Instruction.h"

class ASTNode;

class Function {
	private:
		std::vector<std::shared_ptr<Instruction>> instructions;

	public:
		const ASTNode &source;
		int nextVariable = 0;

		Function(const ASTNode &source_): source(source_) {}

		std::string compile();
};
