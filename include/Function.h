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
struct Scope;
struct WhyInstruction;

class Function {
	public:
		Program &program;
		std::string name = "???";
		std::vector<std::shared_ptr<CmmInstruction>> cmm;
		std::vector<std::shared_ptr<WhyInstruction>> why;
		std::map<std::string, VariablePtr> variables;
		/** Offsets are relative to the value in the frame pointer right after the stack pointer is written to it in the
		 *  prologue. */
		std::map<VariablePtr, size_t> stackOffsets;
		std::map<const ASTNode *, std::shared_ptr<Scope>> scopes;

		const ASTNode *source = nullptr;
		int nextVariable = 0;

		Function(Program &, const ASTNode *);

		std::vector<std::string> stringify();
		void compile();
		VregPtr newVar();
		VregPtr precolored(int reg);
};
