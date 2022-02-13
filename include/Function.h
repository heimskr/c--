#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Type.h"
#include "Variable.h"

class ASTNode;
struct CmmInstruction;
struct Program;
struct WhyInstruction;

class Function {
	public:
		Program &program;
		std::string name = "???";
		std::vector<std::shared_ptr<CmmInstruction>> cmm;
		std::vector<std::shared_ptr<WhyInstruction>> why;
		std::map<std::string, Variable> variables;

		const ASTNode *source = nullptr;
		int nextVariable = 0;

		Function(Program &, const ASTNode *);

		std::vector<std::string> stringify();
		void compile();
		VregPtr newVar() { return std::make_shared<VirtualRegister>(*this); }
};
