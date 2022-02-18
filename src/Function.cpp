#include <iostream>

#include "ASTNode.h"
#include "Casting.h"
#include "ColoringAllocator.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Lexer.h"
#include "Scope.h"
#include "Util.h"
#include "WhyInstructions.h"

#define DEBUG_SPILL

Function::Function(Program &program_, const ASTNode *source_):
program(program_), source(source_),
selfScope(MultiScope::make(GlobalScope::make(program), FunctionScope::make(*this))) {
	if (source) {
		name = *source->at(0)->lexerInfo;
		returnType = TypePtr(Type::get(*source->at(1)));
	} else
		returnType = VoidType::make();
	scopes.emplace(0, selfScope);
}

std::vector<std::string> Function::stringify(bool colored) {
	std::vector<std::string> out;
	if (colored) {
		for (const auto &instruction: instructions)
			for (const std::string &line: instruction->colored())
				out.push_back(line);
	} else {
		for (const auto &instruction: instructions)
			for (const std::string &line: std::vector<std::string>(*instruction))
				out.push_back(line);
	}
	return out;
}

void Function::compile() {
	const bool is_init = name == ".init";

	if (!is_init) {
		if (isBuiltin())
			return;

		if (!source)
			throw std::runtime_error("Can't compile " + name + ": no source node");

		if (source->size() != 4)
			throw std::runtime_error("Expected 4 nodes in " + name + "'s source node, found " +
				std::to_string(source->size()));

		int i = 0;
		for (const ASTNode *child: *source->at(2)) {
			const std::string &argument_name = *child->lexerInfo;
			arguments.push_back(argument_name);
			VariablePtr argument = Variable::make(argument_name, TypePtr(Type::get(*child->front())), *this);
			argument->init();
			argumentMap.emplace(argument_name, argument);
			if (i < Why::argumentCount) {
				argument->reg = Why::argumentOffset + i;
			} else
				throw std::runtime_error("Functions with greater than " + std::to_string(Why::argumentCount) +
					" arguments are currently unsupported.");
			if (selfScope->lookup(argument_name))
				throw NameConflictError(argument_name);
			variables.emplace(argument_name, argument);
			++i;
		}

		for (const ASTNode *child: *source->at(3))
			compile(*child);
	}

	extractBlocks();
	split();
	updateVregs();
	makeCFG();
	computeLiveness();
	ColoringAllocator allocator(*this);
	Allocator::Result result;
	do
		result = allocator.attempt();
	while (result != Allocator::Result::Success);

	auto rt = precolored(Why::returnAddressOffset);

	if (!is_init) {
		auto gp_regs = usedGPRegisters();
		auto fp = precolored(Why::framePointerOffset), sp = precolored(Why::stackPointerOffset);
		addFront<MoveInstruction>(sp, fp);
		for (int reg: gp_regs)
			addFront<StackPushInstruction>(precolored(reg));
		addFront<StackPushInstruction>(fp);
		addFront<StackPushInstruction>(rt);
		add<Label>("." + name + ".e");
		for (int reg: gp_regs)
			add<StackPopInstruction>(precolored(reg));
		add<StackPopInstruction>(fp);
		add<StackPopInstruction>(rt);
	}

	add<JumpRegisterInstruction>(rt, false);
}

std::set<int> Function::usedGPRegisters() const {
	std::set<int> out;
	for (const auto &instruction: instructions) {
		for (const auto &var: instruction->getRead())
			if (Why::isGeneralPurpose(var->reg))
				out.insert(var->reg);
		for (const auto &var: instruction->getWritten())
			if (Why::isGeneralPurpose(var->reg))
				out.insert(var->reg);
	}
	return out;
}

VregPtr Function::newVar() {
	return std::make_shared<VirtualRegister>(*this)->init();
}

ScopePtr Function::newScope(int *id_out) {
	const int new_id = ++nextBlock;
	auto new_scope = std::make_shared<BlockScope>(selfScope);
	scopes.try_emplace(new_id, new_scope);
	if (id_out)
		*id_out = new_id;
	return new_scope;
}

VregPtr Function::precolored(int reg) {
	auto out = std::make_shared<VirtualRegister>(*this)->init();
	out->reg = reg;
	out->precolored = true;
	return out;
}

size_t Function::addToStack(VariablePtr variable) {
	if (stackOffsets.count(variable) != 0)
		throw std::runtime_error("Variable already on the stack in function " + name + ": " + variable->name);
	stackOffsets.emplace(variable, stackUsage += variable->type->getSize());
	return stackUsage;
}

void Function::compile(const ASTNode &node) {
	switch (node.symbol) {
		case CMMTOK_COLON: {
			const std::string &var_name = *node.front()->lexerInfo;
			if (selfScope->lookup(var_name))
				throw NameConflictError(var_name, node.front()->location);
			VariablePtr variable = Variable::make(var_name, TypePtr(Type::get(*node.at(1))), *this);
			variable->init();
			variables.emplace(var_name, variable);
			size_t offset = addToStack(variable);
			if (node.size() == 3) {
				auto expr = ExprPtr(Expr::get(*node.at(2), this));
				expr->compile(variable, *this, selfScope);
				typeCheck(expr->getType(selfScope), variable->type, variable, *this);
				VregPtr fp = precolored(Why::framePointerOffset);
				if (offset == 0) {
					add<StoreRInstruction>(variable, fp);
				} else {
					VregPtr m0 = mx(0);
					add<SubIInstruction>(fp, m0, int(offset));
					add<StoreRInstruction>(variable, m0);
				}
			}
			break;
		}
		case CMMTOK_RETURN:
			ExprPtr(Expr::get(*node.front(), this))->compile(precolored(Why::returnValueOffset), *this,
				selfScope);
			add<JumpInstruction>("." + name + ".e");
			break;
		case CMMTOK_LPAREN:
			ExprPtr(Expr::get(node, this))->compile(nullptr, *this, selfScope);
			break;
		case CMMTOK_WHILE: {
			ExprPtr condition = ExprPtr(Expr::get(*node.front(), this));
			const std::string label = "." + name + "." + std::to_string(++nextBlock);
			const std::string start = label + "s", end = label + "e";
			add<Label>(start);
			auto m0 = mx(0);
			condition->compile(m0, *this, selfScope);
			add<LogicalNotInstruction>(m0);
			add<JumpConditionalInstruction>(end, m0);
			compile(*node.at(1));
			add<JumpInstruction>(start);
			add<Label>(end);
			break;
		}
		case CMM_BLOCK:
			for (const ASTNode *child: node)
				compile(*child);
			break;
		case CMMTOK_IF: {
			const std::string end_label = "." + name + "." + std::to_string(++nextBlock) + "end";
			ExprPtr condition = ExprPtr(Expr::get(*node.front(), this));
			auto m0 = mx(0);
			condition->compile(m0, *this, selfScope);
			add<LogicalNotInstruction>(m0);
			if (node.size() == 3) {
				const std::string else_label = "." + name + "." + std::to_string(nextBlock) + "e";
				add<JumpConditionalInstruction>(else_label, m0);
				compile(*node.at(1));
				add<JumpInstruction>(end_label);
				add<Label>(else_label);
				compile(*node.at(2));
			} else {
				add<JumpConditionalInstruction>(end_label, m0);
				compile(*node.at(1));
			}
			add<Label>(end_label);
			break;
		}
		case CMMTOK_ASSIGN: {
			ExprPtr(Expr::get(node, this))->compile(nullptr, *this, selfScope);
			break;
		}
		default:
			node.debug();
			break;
	}
}

std::list<BasicBlockPtr> & Function::extractBlocks(std::map<std::string, BasicBlockPtr> *map_out) {
	std::map<std::string, BasicBlockPtr> map;
	blocks.clear();
	anons = 0;

	BasicBlockPtr current = BasicBlock::make(*this, name);
	bool waiting = false;
	bool at_first = true;
	bool label_found = false;

	std::vector<std::pair<std::string, std::string>> extra_connections;

	for (const auto &instruction: instructions) {
		if (waiting) {
			if (auto label = instruction->ptrcast<Label>())
				current = BasicBlock::make(*this, label->name);
			else
				current = BasicBlock::make(*this, "." + name + ".anon." + std::to_string(anons++));
			waiting = false;
		}

		const bool is_label = instruction->is<Label>();

		if (is_label) {
			const auto &label = instruction->ptrcast<Label>();
			if (label_found) {
				extra_connections.emplace_back(current->label, label->name);
				blocks.push_back(current);
				map.emplace(current->label, current);
				current = BasicBlock::make(*this, label->name);
				*current += label;
				at_first = true;
				label_found = false;
				continue;
			}
			label_found = true;
		}

		*current += instruction;

		if (instruction->isTerminal()) {
			blocks.push_back(current);
			map.emplace(current->label, current);
			at_first = waiting = true;
			label_found = false;
		} else
			at_first = false;
	}

	if (*current && map.count(current->label) == 0) {
		blocks.push_back(current);
		map.emplace(current->label, current);
	}

	int last_index = -1;
	for (auto &block: blocks) {
		block->index = ++last_index;
		if (!*block)
			continue;
		const auto last = block->instructions.back();
		if (auto *jtype = last->cast<JType>()) {
			if (std::holds_alternative<std::string>(jtype->imm)) {
				const std::string &target_name = std::get<std::string>(jtype->imm);
				if (map.count(target_name) != 0) {
					auto &target = map.at(target_name);
					target->predecessors.insert(block);
					block->successors.insert(target);
				}
			}
		}

		for (const auto &instruction: block->instructions) {
			instruction->parent = block;
			if (const auto *conditional = instruction->cast<JumpConditionalInstruction>()) {
				const std::string &target_name = std::get<std::string>(conditional->imm);
				if (map.count(target_name) != 0) {
					auto &target = map.at(target_name);
					target->predecessors.insert(block);
					block->successors.insert(target);
				}
			}
		}
	}

	if (map_out)
		*map_out = std::move(map);

	return blocks;
}

void Function::relinearize(const std::list<BasicBlockPtr> &block_vec) {
	instructions.clear();
	int last_index = -1;
	for (const auto &block: block_vec)
		for (auto &instruction: block->instructions) {
			instructions.push_back(instruction);
			instruction->parent = block;
			instruction->index = ++last_index;
		}
}

void Function::relinearize() {
	relinearize(blocks);
}

void Function::reindex() {
	int last_index = -1;
	for (auto &instruction: instructions)
		instruction->index = ++last_index;
}

int Function::split(std::map<std::string, BasicBlockPtr> *map) {
	bool changed;
	int count = 0;
	do {
		changed = false;
		for (auto iter = blocks.begin(), end = blocks.end(); iter != end; ++iter) {
			auto &block = *iter;
			if (Why::generalPurposeRegisters < block->countVariables()) {
				// It would be better to split at the point where the unique variable count begins to exceed the
				// maximum, but it's probably more efficient to simply split in the middle.
				BasicBlockPtr new_block = BasicBlock::make(*this, "." + name + ".anon." + std::to_string(anons++));

				for (size_t i = 0, to_remove = block->instructions.size() / 2; i < to_remove; ++i) {
					new_block->instructions.push_front(block->instructions.back());
					block->instructions.pop_back();
				}

				if (map)
					map->emplace(new_block->label, new_block);

				new_block->successors = block->successors;
				block->successors = {new_block};
				new_block->predecessors = {block};

				for (auto &other_block: blocks)
					if (other_block != block && other_block->predecessors.count(block) != 0) {
						other_block->predecessors.erase(block);
						other_block->predecessors.insert(new_block);
					}

				blocks.insert(++iter, new_block);
				changed = true;
				++count;
				break;
			}
		}
	} while (changed);

	if (count != 0) {
		int last_index = -1;
		for (auto &block: blocks)
			block->index = ++last_index;
		relinearize();
		makeCFG();
	}

	return count;
}

void Function::computeLiveness() {
	for (auto &block: blocks) {
		block->liveIn.clear();
		block->liveOut.clear();
	}

	for (auto &block: blocks) {
		block->cacheReadWritten();
		for (auto vreg: block->readCache)
			upAndMark(block, vreg);
	}
}

void Function::upAndMark(BasicBlockPtr block, VregPtr vreg) {
	for (const auto &instruction: block->instructions)
		if (instruction->doesWrite(vreg))
			return;

	if (block->liveIn.count(vreg) != 0)
		return;

	block->liveIn.insert(vreg);

	try {
		for (const Node *node: bbNodeMap.at(block.get())->in()) {
			BasicBlockPtr p = node->get<std::weak_ptr<BasicBlock>>().lock();
			p->liveOut.insert(vreg);
			upAndMark(p, vreg);
		}
	} catch (std::out_of_range &) {
		debug();
		std::cerr << "Couldn't find block " << block->label << ". bbNodeMap (" << bbNodeMap.size() << "):";
		for (const auto &pair: bbNodeMap)
			std::cerr << " " << pair.first->label;
		std::cerr << "\n";
		throw;
	}
}

bool Function::spill(VregPtr vreg) {
	// Right after the definition of the vreg to be spilled, store its value onto the stack in the proper location.
	// For each use of the original vreg, replace the original vreg with a new vreg, and right before the use insert a
	// definition for the vreg by loading it from the stack.

	if (vreg->writers.empty()) {
		debug();
		warn() << *vreg;
		throw std::runtime_error("Can't spill vreg " + vreg->regOrID() + " in function " + name +
			": no definitions");
	}

	bool out = false;
	const size_t location = getSpill(vreg, true);

	for (std::weak_ptr<WhyInstruction> weak_definition: vreg->writers) {
		WhyPtr definition = weak_definition.lock();
		auto store = std::make_shared<StackStoreInstruction>(vreg, int(location));
		auto next = after(definition);
		bool should_insert = true;

		// Skip comments.
		while (next && dynamic_cast<Comment *>(next.get()) != nullptr)
			next = after(next);

		if (next) {
			auto other_store = std::dynamic_pointer_cast<StackStoreInstruction>(next);
			if (other_store && *other_store == *store)
				should_insert = false;
		}

		if (should_insert) {
			insertAfter(definition, store, false);
			insertBefore(store, std::make_shared<Comment>("Spill: stack store for " + vreg->regOrID() +
				" into location=" + std::to_string(location)));
			VregPtr new_var = mx(6, definition);
			definition->replaceWritten(vreg, new_var);
			store->setSource(new_var);
			out = true;
		} else
			addComment(after(definition), "Spill: no store inserted here for " + vreg->regOrID());
	}

	for (auto iter = instructions.begin(), end = instructions.end(); iter != end; ++iter) {
		WhyPtr &instruction = *iter;
		if (instruction->doesRead(vreg)) {
			VregPtr new_vreg = newVar();
			const bool replaced = instruction->replaceRead(vreg, new_vreg);
			if (replaced) {
				auto load = std::make_shared<StackLoadInstruction>(new_vreg, location);
				insertBefore(instruction, load);
				addComment(load, "Spill: stack load: location=" + std::to_string(location));
				out = true;
				markSpilled(new_vreg);
			} else
				virtualRegisters.erase(new_vreg);
		}
	}

	markSpilled(vreg);
	split();
	computeLiveness();
	return out;
}

size_t Function::getSpill(VregPtr variable, bool create, bool *created) {
	if (created)
		*created = false;
	if (spillLocations.count(variable) != 0)
		return spillLocations.at(variable);
	if (create) {
		if (created)
			*created = true;
		// return addToStack(variable, StackLocation::Purpose::Spill);
		const size_t out = spillLocations[variable] = stackUsage;
		spilledVregs.insert(variable);
		stackUsage += Why::wordSize;
		return out;
	}
	throw std::out_of_range("Couldn't find a spill location for " + variable->regOrID());
}

void Function::markSpilled(VregPtr vreg) {
	spilledVregs.insert(vreg);
}

bool Function::isSpilled(VregPtr vreg) const {
	return spilledVregs.count(vreg) != 0;
}

bool Function::canSpill(VregPtr vreg) {
	// if (vreg->writers.empty() || isSpilled(vreg))
	// 	return false;
	if (vreg->writers.empty()) {
		// std::cerr << "Can't spill " << *vreg << ": no definers\n";
		return false;
	}
	if (isSpilled(vreg)) {
		std::cerr << "Can't spill " << *vreg << ": already spilled\n";
		return false;
	}

	// If the only definition is a stack store, the variable can't be spilled.
	if (vreg->writers.size() == 1) {
		auto single_def = vreg->writers.begin()->lock();
		if (!single_def) {
			warn() << *vreg << '\n';
			throw std::runtime_error("Can't lock single writer of instruction");
		}
		auto *store = dynamic_cast<StackStoreInstruction *>(single_def.get());
		if (store && store->leftSource == vreg) {
			std::cerr << "Can't spill " << *vreg << ": only definer is a stack store\n";
			return false;
		}
	}

	for (auto weak_definition: vreg->writers) {
		auto definition = weak_definition.lock();

		bool created;
		const size_t location = getSpill(vreg, true, &created);
		auto store = std::make_shared<StackStoreInstruction>(vreg, int(location));
		auto next = after(definition);
		bool should_insert = true;

		// Skip comments.
		while (next && next->is<Comment>())
			next = after(next);

		if (next) {
			auto other_store = next->ptrcast<StackStoreInstruction>();
			if (other_store && *other_store == *store)
				should_insert = false;
		}

		if (created) {
			// Undo addition to stack.
			stackUsage -= Why::wordSize;
			spillLocations.erase(vreg);
			spilledVregs.erase(vreg);
		}

		if (should_insert)
			return true;
	}

	for (auto iter = instructions.begin(), end = instructions.end(); iter != end; ++iter) {
		const auto &instruction = *iter;
		if (instruction->doesRead(vreg) && instruction->canReplaceRead(vreg))
			return true;
	}

	return false;
}

std::set<std::shared_ptr<BasicBlock>> Function::getLive(VregPtr var,
std::function<std::set<VregPtr> &(const std::shared_ptr<BasicBlock> &)> getter) const {
	std::set<std::shared_ptr<BasicBlock>> out;
	for (const auto &block: blocks)
		if (getter(block).count(var) != 0)
			out.insert(block);
	return out;
}

std::set<std::shared_ptr<BasicBlock>> Function::getLiveIn(VregPtr var) const {
	return getLive(var, [&](const auto &block) -> auto & {
		return block->liveIn;
	});
}

std::set<std::shared_ptr<BasicBlock>> Function::getLiveOut(VregPtr var) const {
	return getLive(var, [&](const auto &block) -> auto & {
		return block->liveOut;
	});
}

void Function::updateVregs() {
	for (auto &var: virtualRegisters) {
		var->readingBlocks.clear();
		var->writingBlocks.clear();
		var->readers.clear();
		var->writers.clear();
	}

	for (const auto &block: blocks)
		for (const auto &instruction: block->instructions) {
			for (auto &var: instruction->getRead()) {
				var->readingBlocks.insert(block);
				var->readers.insert(instruction);
			}
			for (auto &var: instruction->getWritten()) {
				var->writingBlocks.insert(block);
				var->writers.insert(instruction);
			}
		}
}

WhyPtr Function::after(WhyPtr instruction) {
	auto iter = std::find(instructions.begin(), instructions.end(), instruction);
	return ++iter == instructions.end()? nullptr : *iter;
}

WhyPtr Function::insertAfter(WhyPtr base, WhyPtr new_instruction, bool reindex) {
	BasicBlockPtr block = base->parent.lock();
	if (!block) {
		std::cerr << "\e[31;1m!\e[0m " << base->joined(true, "; ") << '\n';
		throw std::runtime_error("Couldn't lock instruction's parent block");
	}

	// if (new_instruction->debugIndex == -1)
	// 	new_instruction->debugIndex = base->debugIndex;

	if (reindex)
		// There used to be a + 1 here, but I removed it because I believe it gets incremented in the loop shortly
		// before the end of this function anyway.
		new_instruction->index = base->index;
	new_instruction->parent = base->parent;
	std::list<WhyPtr>::iterator iter;

	if (base == instructions.back()) {
		instructions.push_back(new_instruction);
		block->instructions.push_back(new_instruction);
	} else {
		if (base == block->instructions.back()) {
			block->instructions.push_back(new_instruction);
		} else {
			iter = std::find(block->instructions.begin(), block->instructions.end(), base);
			++iter;
			block->instructions.insert(iter, new_instruction);
		}

		iter = std::find(instructions.begin(), instructions.end(), base);
		instructions.insert(++iter, new_instruction);

		if (reindex)
			for (auto end = instructions.end(); iter != end; ++iter)
				++(*iter)->index;
	}

	return new_instruction;
}

WhyPtr Function::insertBefore(WhyPtr base, WhyPtr new_instruction, bool reindex, bool linear_warn,
                              bool *should_relinearize_out) {
	BasicBlockPtr block = base->parent.lock();
	if (!block) {
		error() << *base << "\n";
		throw std::runtime_error("Couldn't lock instruction's parent block");
	}

	if (&block->function != this)
		throw std::runtime_error("Block parent isn't equal to this in Function::insertBefore");

	// if (new_instruction->debugIndex == -1)
	// 	new_instruction->debugIndex = base->debugIndex;

	new_instruction->parent = base->parent;

	auto instructionIter = std::find(instructions.begin(), instructions.end(), base);
	if (linear_warn && instructionIter == instructions.end()) {
		warn() << "Couldn't find instruction in instructions field of function " << name << ": " << *base << '\n';
		throw std::runtime_error("Couldn't find instruction in instructions field of function " + name);
	}

	auto blockIter = std::find(block->instructions.begin(), block->instructions.end(), base);
	if (blockIter == block->instructions.end()) {
		warn() << "Couldn't find instruction in block " << block->label << " of function " << name << ": " << *base
		       << '\n';
		std::cerr << "Index: " << block->index << '\n';
		for (const auto &subblock: blocks)
			std::cerr << subblock->label << '[' << subblock->index << "] ";
		std::cerr << '\n';
		for (const auto &block_instruction: block->instructions)
			std::cerr << '\t' << *block_instruction << '\n';
		throw std::runtime_error("Instruction not found in block");
	}

	const bool can_insert_linear = instructionIter != instructions.end();
	if (can_insert_linear) {
		instructions.insert(instructionIter, new_instruction);
		if (should_relinearize_out)
			*should_relinearize_out = false;
	} else if (should_relinearize_out)
		*should_relinearize_out = true;

	block->instructions.insert(blockIter, new_instruction);

	if (reindex && can_insert_linear) {
		new_instruction->index = base->index;
		for (auto end = instructions.end(); instructionIter != end; ++instructionIter)
			++(*instructionIter)->index;
	}

	return new_instruction;
}

void Function::addComment(const std::string &comment) {
	add<Comment>(comment);
}

void Function::addComment(WhyPtr base, const std::string &comment) {
	insertBefore(base, Comment::make(comment));
}

VregPtr Function::mx(int n, BasicBlockPtr writer) {
	auto out = precolored(Why::assemblerOffset + n);
	if (writer)
		out->writingBlocks.insert(writer);
	return out;
}

VregPtr Function::mx(int n, InstructionPtr writer) {
	return mx(n, writer->parent.lock());
}

VregPtr Function::mx(InstructionPtr writer) {
	return mx(0, writer->parent.lock());
}

void Function::debug() const {
	for (const auto &block: blocks) {
		std::cerr << "\e[1;32m@" << block->label;
		if (!block->predecessors.empty()) {
			std::cerr << " <-";
			for (const auto &pred: block->predecessors)
				std::cerr << ' ' << pred.lock()->label;
		}
		if (!block->successors.empty()) {
			std::cerr << " ->";
			for (const auto &succ: block->successors)
				std::cerr << ' ' << succ.lock()->label;
		}
		std::cerr << "\e[0m (" << block->countVariables() << ")\n";
		for (const auto &instruction: block->instructions)
			std::cerr << '\t' << instruction->joined() << '\n';
		std::cerr << "\e[36mLive-in: \e[1m";
		std::set<int> in, out;
		for (const auto &var: block->liveIn) in.insert(var->id);
		for (int id: in) std::cerr << ' ' << id;
		std::cerr << "\e[22m\nLive-out:\e[1m";
		for (const auto &var: block->liveOut) out.insert(var->id);
		for (int id: out) std::cerr << ' ' << id;
		std::cerr << "\e[0m\n\n";
	}
}

Graph & Function::makeCFG() {
	cfg.clear();
	cfg.name = "CFG for " + name;
	bbNodeMap.clear();

	// First pass: add all the nodes.
	for (BasicBlockPtr &block: blocks) {
		const std::string &label = block->label;
		cfg += label;
		Node &node = cfg[label];
		node.data = std::weak_ptr<BasicBlock>(block);
		block->node = &node;
		bbNodeMap.emplace(block.get(), &node);
	}

	cfg += "exit";

	bool exit_linked = false;

	// Second pass: connect all the nodes.
	for (BasicBlockPtr &block: blocks) {
		const std::string &label = block->label;
		for (const auto &weak_pred: block->predecessors) {
			auto pred = weak_pred.lock();
			if (!pred)
				throw std::runtime_error("Couldn't lock predecessor of " + label);
			if (cfg.hasLabel(pred->label)) {
				cfg.link(pred->label, label);
			} else {
				warn() << "Predicate \e[1m" << pred->label << "\e[22m doesn't correspond to any CFG node in "
							"function \e[1m" << name << "\e[22m\n";
				for (const auto &pair: cfg)
					std::cerr << "- " << pair.first << '\n';
			}
		}

		if (!block->instructions.empty()) {
			auto &back = block->instructions.back();
			if (auto *jump = back->cast<JumpInstruction>()) {
				if (jump->imm == label) {
					// The block unconditionally branches to itself, meaning it's an infinite loop.
					// Let's pretend that it's connected to the exit.
					cfg.link(label, "exit");
					exit_linked = true;
				}
			}
		}
	}

	if (!exit_linked)
		// Sometimes there's an infinite loop without a block unconditionally branching to itself. The CFG might
		// look like ([Start, A, B, C, Exit] : [Start -> A, A -> B, B -> C, C -> A]). In this case, we just pretend
		// that the final block links to the exit node.
		cfg.link(blocks.back()->label, "exit");

	return cfg;
}
