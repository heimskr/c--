#pragma once

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "BasicBlock.h"
#include "Graph.h"
#include "Type.h"
#include "Variable.h"

class ASTNode;
struct Instruction;
struct Program;
struct Scope;
struct WhyInstruction;

using WhyPtr = std::shared_ptr<WhyInstruction>;

class Function {
	private:
		int nextBlock = 0, anons = 0;
		void compile(const ASTNode &, const std::string &break_label = "", const std::string &continue_label = "");
		void extractArguments();

	public:
		enum class Attribute {Naked};
		Program &program;
		std::string name = "???";
		std::list<WhyPtr> instructions;
		std::map<std::string, VariablePtr> variables;
		std::set<VregPtr> virtualRegisters;
		/** Offsets are relative to the value in the frame pointer right after the stack pointer is written to it in the
		 *  prologue. */
		std::map<VariablePtr, size_t> stackOffsets;
		std::map<VregPtr, size_t> spillLocations;
		std::set<VregPtr> spilledVregs;
		std::map<int, std::shared_ptr<Scope>> scopes;
		std::list<BasicBlockPtr> blocks;
		Graph cfg;
		std::unordered_set<Attribute> attributes;

		TypePtr returnType;
		std::vector<std::string> arguments;
		std::map<std::string, VariablePtr> argumentMap;
		const ASTNode *source = nullptr;
		int nextVariable = 0;
		size_t stackUsage = 0;
		std::shared_ptr<Scope> selfScope;
		/** Maps basic blocks to their corresponding CFG nodes. */
		std::unordered_map<const BasicBlock *, Node *> bbNodeMap;
		std::vector<std::shared_ptr<Scope>> scopeStack;

		Function(Program &, const ASTNode *);

		void setArguments(const std::vector<std::pair<std::string, TypePtr>> &);

		std::shared_ptr<Scope> currentScope() const { return scopeStack.back(); }

		std::vector<std::string> stringify(bool colored = false);

		void compile();

		std::set<int> usedGPRegisters() const;

		VregPtr newVar(TypePtr = nullptr);

		std::shared_ptr<Scope> newScope(int *id_out = nullptr);

		VregPtr precolored(int reg);

		size_t addToStack(VariablePtr);

		std::list<BasicBlockPtr> & extractBlocks(std::map<std::string, BasicBlockPtr> * = nullptr);

		void relinearize(const std::list<BasicBlockPtr> &);

		void relinearize();

		void reindex();

		/** If any blocks have more unique variables than the number of temporary registers supported by the ISA, this
		 *  function will split the blocks until all blocks can fit their variables in temporary registers. Returns
		 *  the number of new blocks created. */
		int split(std::map<std::string, BasicBlockPtr> * = nullptr);

		void computeLiveness();

		void upAndMark(BasicBlockPtr, VregPtr);

		/** Tries to spill a variable. Returns true if any instructions were inserted. */
		bool spill(VregPtr);

		/** Finds a spill stack location for a variable. */
		size_t getSpill(VregPtr, bool create = false, bool *created = nullptr);

		void markSpilled(VregPtr);

		bool isSpilled(VregPtr) const;

		bool canSpill(VregPtr);

		std::set<std::shared_ptr<BasicBlock>> getLive(VregPtr,
			std::function<std::set<VregPtr> &(const std::shared_ptr<BasicBlock> &)>) const;

		/** Returns a set of all blocks where a given variable or any of its aliases are live-in. */
		std::set<std::shared_ptr<BasicBlock>> getLiveIn(VregPtr) const;

		/** Returns a set of all blocks where a given variable or any of its aliases are live-out. */
		std::set<std::shared_ptr<BasicBlock>> getLiveOut(VregPtr) const;

		/** Resets the readingBlocks and writingBlocks fields of all virtual registers used in the function. */
		void updateVregs();

		/** Returns a pointer to the instruction following a given instruction. */
		WhyPtr after(WhyPtr);

		/** Inserts one instruction after another. Returns the inserted instruction. */
		WhyPtr insertAfter(WhyPtr base, WhyPtr new_instruction, bool reindex = true);

		/** Inserts one instruction before another. Returns the inserted instruction. */
		WhyPtr insertBefore(WhyPtr base, WhyPtr new_instruction, bool reindex = true, bool linear_warn = true,
			bool *should_relinearize_out = nullptr);

		bool isBuiltin() const { return !name.empty() && (name.front() == '.' || name.front() == '`'); }

		template <typename T, typename... Args>
		std::shared_ptr<T> add(Args &&...args) {
			return std::dynamic_pointer_cast<T>(instructions.emplace_back(new T(std::forward<Args>(args)...)));
		}

		template <typename T, typename... Args>
		std::shared_ptr<T> addFront(Args &&...args) {
			return std::dynamic_pointer_cast<T>(instructions.emplace_front(new T(std::forward<Args>(args)...)));
		}

		void addComment(const std::string &);
		void addComment(WhyPtr base, const std::string &);

		VregPtr mx(int = 0, BasicBlockPtr writer = nullptr);
		VregPtr mx(int, std::shared_ptr<Instruction> writer);
		VregPtr mx(std::shared_ptr<Instruction> writer);

		int getNextBlock() { return ++nextBlock; }

		void debug() const;

		Graph & makeCFG();

		bool isNaked() const;

		void checkNaked(const ASTNode &) const;
};
