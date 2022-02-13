#pragma once

#include <memory>
#include <string>
#include <vector>

class ASTNode;
struct CmmInstruction;
struct WhyInstruction;

class Function {
	public:
		std::vector<std::shared_ptr<CmmInstruction>> cmm;
		std::vector<std::shared_ptr<WhyInstruction>> why;

		const ASTNode &source;
		int nextVariable = 0;

		Function(const ASTNode &source_): source(source_) {}

		std::string compile();
};
