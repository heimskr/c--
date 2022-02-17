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
		int nextBlock = 0, anons = 0;
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
		std::list<BasicBlockPtr> blocks;

		TypePtr returnType;
		std::vector<std::string> arguments;
		std::map<std::string, VariablePtr> argumentMap;
		const ASTNode *source = nullptr;
		int nextVariable = 0;
		size_t stackUsage = 0;
		// size_t walkCount = 0;
		std::shared_ptr<Scope> selfScope;

		Function(Program &, const ASTNode *);

		std::vector<std::string> stringify();

		void compile();

		VregPtr newVar();

		std::shared_ptr<Scope> newScope(int *id_out = nullptr);

		VregPtr precolored(int reg);

		size_t addToStack(VariablePtr);

		std::list<BasicBlockPtr> & extractBlocks(std::map<std::string, BasicBlockPtr> * = nullptr);

		void relinearize(const std::list<BasicBlockPtr> &);

		void relinearize();

		/** If any blocks have more unique variables than the number of temporary registers supported by the ISA, this
		 *  function will split the blocks until all blocks can fit their variables in temporary registers. Returns
		 *  the number of new blocks created. */
		int split(std::map<std::string, BasicBlockPtr> * = nullptr);

		void computeLiveness();

		/** Tries to spill a variable. Returns true if any instructions were inserted. */
		bool spill(VregPtr, bool doDebug = false);

		bool canSpill(VariablePtr);

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
