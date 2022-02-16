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
	private:
		// int nextBlock = 0;

	public:
		Program &program;
		std::string name = "???";
		// std::vector<std::shared_ptr<CmmInstruction>> cmm;
		std::vector<std::shared_ptr<WhyInstruction>> why;
		std::map<std::string, VariablePtr> variables;
		/** Offsets are relative to the value in the frame pointer right after the stack pointer is written to it in the
		 *  prologue. */
		std::map<VariablePtr, size_t> stackOffsets;
		std::map<const ASTNode *, std::shared_ptr<Scope>> scopes;

		TypePtr returnType;
		std::vector<std::string> arguments;
		const ASTNode *source = nullptr;
		int nextVariable = 0;
		size_t stackUsage = 0;
		std::shared_ptr<Scope> selfScope;

		Function(Program &, const ASTNode *);

		std::vector<std::string> stringify();
		void compile();
		VregPtr newVar();
		VregPtr precolored(int reg);
		void addToStack(VariablePtr);

		template <typename T, typename... Args>
		void add(Args &&...args) {
			why.emplace_back(new T(std::forward<Args>(args)...));
		}
};
