#pragma once

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "BasicBlock.h"
#include "Type.h"
#include "Variable.h"

class ASTNode;
struct CmmInstruction;
struct Program;
struct Scope;
struct WhyInstruction;

class Function {
	private:
		int nextBlock = 0;
		void compile(const ASTNode &);

	public:
		Program &program;
		std::string name = "???";
		// std::vector<std::shared_ptr<CmmInstruction>> cmm;
		std::deque<std::shared_ptr<WhyInstruction>> why;
		std::map<std::string, VariablePtr> variables;
		/** Offsets are relative to the value in the frame pointer right after the stack pointer is written to it in the
		 *  prologue. */
		std::map<VariablePtr, size_t> stackOffsets;
		std::map<int, std::shared_ptr<Scope>> scopes;

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
		std::shared_ptr<Scope> newScope(int *id_out = nullptr);
		VregPtr precolored(int reg);
		size_t addToStack(VariablePtr);
		// std::set<VregPtr> gatherVariables() const;
		std::vector<BasicBlockPtr> extractBlocks(std::map<std::string, BasicBlockPtr> * = nullptr);

		bool isBuiltin() const { return !name.empty() && name.front() == '.'; }

		template <typename T, typename... Args>
		std::shared_ptr<T> add(Args &&...args) {
			return std::dynamic_pointer_cast<T>(why.emplace_back(new T(std::forward<Args>(args)...)));
		}

		template <typename T, typename... Args>
		std::shared_ptr<T> addFront(Args &&...args) {
			return std::dynamic_pointer_cast<T>(why.emplace_front(new T(std::forward<Args>(args)...)));
		}

		void addComment(const std::string &);
		VregPtr mx(int = 0);

};
