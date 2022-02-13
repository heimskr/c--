#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Variable.h"

class ASTNode;
struct CmmInstruction;
struct WhyInstruction;

class Function {
	public:
		std::vector<std::shared_ptr<CmmInstruction>> cmm;
		std::vector<std::shared_ptr<WhyInstruction>> why;

		const ASTNode *source = nullptr;
		int nextVariable = 0;

		Function(const ASTNode *source_): source(source_) {}

		std::vector<std::string> compile();
		VregPtr newVar() { return std::make_shared<VirtualRegister>(*this); }
};
