#include <iostream>

#include "ASTNode.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Lexer.h"
#include "Scope.h"
#include "Util.h"
#include "WhyInstructions.h"

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

std::vector<std::string> Function::stringify() {
	std::vector<std::string> out;
	for (const auto &instruction: why)
		for (const std::string &line: std::vector<std::string>(*instruction))
			out.push_back(line);
	return out;
}

void Function::compile() {
	if (isBuiltin())
		return;

	if (!source)
		throw std::runtime_error("Can't compile " + name + ": no source node");

	if (source->size() != 4)
		throw std::runtime_error("Expected 4 nodes in " + name + "'s source node, found " +
			std::to_string(source->size()));

	std::cerr << "\n\e[32mCompiling " << name << "\e[39m\n\n";

	int i = 0;
	for (const ASTNode *child: *source->at(2)) {
		const std::string &argument_name = *child->lexerInfo;
		arguments.push_back(argument_name);
		VariablePtr argument = Variable::make(argument_name, TypePtr(Type::get(*child->front())), this);
		if (i < Why::argumentCount) {
			argument->reg = Why::argumentOffset + i;
		} else
			throw std::runtime_error("Functions with greater than " + std::to_string(Why::argumentCount) + " arguments "
				"are currently unsupported.");
		if (selfScope->lookup(argument_name))
			throw NameConflictError(argument_name);
		variables.emplace(argument_name, argument);
		++i;
	}

	for (const ASTNode *child: *source->at(3))
		compile(*child);

	auto fp = precolored(Why::framePointerOffset), rt = precolored(Why::returnAddressOffset);
	auto sp = precolored(Why::stackPointerOffset);
	addFront<MoveInstruction>(sp, fp);
	// TODO: push all used temporaries here
	addFront<StackPushInstruction>(fp);
	addFront<StackPushInstruction>(rt);
	add<Label>("." + name + ".e");
	// TODO: pop all used temporaries here
	add<StackPopInstruction>(fp);
	add<StackPopInstruction>(rt);
	add<JumpRegisterInstruction>(rt, false);

	std::cerr << "<BasicBlocks>\n";
	std::map<std::string, BasicBlockPtr> block_map;
	extractBlocks(&block_map);
	std::cerr << "Split: " << split(&block_map) << '\n';
	std::cerr << "Names:"; for (const auto &[name, block]: block_map) std::cerr << ' ' << name; std::cerr << '\n';
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
			for (const std::string &str: std::vector<std::string>(*instruction))
				std::cerr << "\t\e[1m" << str << "\e[0m\n";
		std::cerr << '\n';
	}
	std::cerr << "</BasicBlocks>\n";

}

VregPtr Function::newVar() {
	return std::make_shared<VirtualRegister>(*this);
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
	auto out = std::make_shared<VirtualRegister>(*this);
	out->reg = reg;
	return out;
}

size_t Function::addToStack(VariablePtr variable) {
	if (stackOffsets.count(variable) != 0)
		throw std::runtime_error("Variable already on the stack in function " + name + ": " + variable->name);
	stackOffsets.emplace(variable, stackUsage);
	size_t out = stackUsage;
	stackUsage += variable->type->getSize();
	return out;
}

void Function::compile(const ASTNode &node) {
	switch (node.symbol) {
		case CMMTOK_COLON: {
			const std::string &var_name = *node.front()->lexerInfo;
			if (selfScope->lookup(var_name))
				throw NameConflictError(var_name, node.front()->location);
			VariablePtr variable = Variable::make(var_name, TypePtr(Type::get(*node.at(1))), this);
			variables.emplace(var_name, variable);
			size_t offset = addToStack(variable);
			if (node.size() == 3) {
				ExprPtr(Expr::get(*node.at(2), this))->compile(variable, *this, selfScope);
				VregPtr fp = precolored(Why::framePointerOffset);
				if (offset == 0) {
					add<StoreRInstruction>(variable, fp);
				} else {
					VregPtr m0 = mx(0);
					add<AddIInstruction>(fp, m0, int(offset));
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

	BasicBlockPtr current = BasicBlock::make(name);
	bool waiting = false;
	bool at_first = true;
	bool label_found = false;

	std::vector<std::pair<std::string, std::string>> extra_connections;

	for (const auto &instruction: why) {
		if (waiting) {
			if (auto label = instruction->ptrcast<Label>())
				current = BasicBlock::make(label->name);
			else
				current = BasicBlock::make("." + name + ".anon." + std::to_string(anons++));
			waiting = false;
		}

		const bool is_label = instruction->is<Label>();

		if (is_label) {
			const auto &label = instruction->ptrcast<Label>();
			if (label_found) {
				extra_connections.emplace_back(current->label, label->name);
				blocks.push_back(current);
				map.emplace(current->label, current);
				current = BasicBlock::make(label->name);
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

	for (const auto &block: blocks) {
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
	why.clear();
	for (const auto &block: block_vec)
		for (const auto &instruction: block->instructions)
			why.push_back(instruction);
}

void Function::relinearize() {
	relinearize(blocks);
}

bool Function::split(std::map<std::string, BasicBlockPtr> *map) {
	bool changed, any_split = false;
	do {
		changed = false;
		for (auto iter = blocks.begin(), end = blocks.end(); iter != end; ++iter) {
			auto &block = *iter;
			if (Why::generalPurposeRegisters < block->countVariables()) {
				// It would be better to split at the point where the unique variable count begins to exceed the
				// maximum, but it's probably more efficient to simply split in the middle.
				BasicBlockPtr new_block = BasicBlock::make("." + name + ".anon." + std::to_string(anons++));

				for (size_t i = 0, to_remove = block->instructions.size() / 2; i < to_remove; ++i) {
					new_block->instructions.push_front(block->instructions.back());
					block->instructions.pop_back();
				}

				if (map)
					map->emplace(new_block->label, new_block);

				new_block->successors = block->successors;
				block->successors = {new_block};
				new_block->predecessors = {block};

				blocks.insert(++iter, new_block);
				changed = any_split = true;
				break;
			}
		}
	} while (changed);

	return any_split;
}

void Function::addComment(const std::string &comment) {
	add<Comment>(comment);
}

VregPtr Function::mx(int n) {
	return precolored(Why::assemblerOffset + n);
}
