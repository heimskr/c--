#include <iostream>

#include "ASTNode.h"
#include "Casting.h"
#include "ColoringAllocator.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
#include "Scope.h"
#include "Util.h"
#include "WhyInstructions.h"
#include "wasm/Nodes.h"

#define DEBUG_SPILL

Function::Function(Program &program_, const ASTNode *source_):
program(program_), source(source_), selfScope(FunctionScope::make(*this, GlobalScope::make(program))) {
	if (source) {
		name = *source->text;
		returnType = TypePtr(Type::get(*source->at(0), program));
	} else
		returnType = VoidType::make();
	scopes.emplace(0, selfScope);
	scopeStack.push_back(selfScope);
	extractArguments();
}

std::vector<std::string> Function::stringify(bool colored) const {
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

std::string Function::mangle() const {
	if (!structParent && (name == "main" || isBuiltin()))
		return name;
	std::stringstream out;
	if (structParent)
		out << '.' << structParent->name.size() << structParent->name;
	else
		out << '_';
	out << name.size() << name << returnType->mangle() << (arguments.size() - argumentMap.count("this"));
	for (const std::string &argument: arguments)
		if (argument != "this")
			out << argumentMap.at(argument)->type->mangle();
	return out.str();
}

void Function::extractArguments() {
	arguments.clear();
	argumentMap.clear();
	if (!source)
		return;

	for (const ASTNode *child: *source->at(1)) {
		const std::string &argument_name = *child->text;
		arguments.push_back(argument_name);
		auto type = TypePtr(Type::get(*child->front(), program));
		if (type->isStruct())
			throw LocatedError(child->location, "Functions cannot directly take structs as function arguments; "
				"use a pointer");
		VariablePtr argument = Variable::make(argument_name, type, *this);
		argument->init();
		argumentMap.emplace(argument_name, argument);
		if (selfScope->doesConflict(argument_name))
			throw NameConflictError(argument_name);
		variables.emplace(argument_name, argument);
	}
}

void Function::setArguments(const std::vector<std::pair<std::string, TypePtr>> &args) {
	arguments.clear();
	argumentMap.clear();
	int i = 0;
	for (const auto &[argument_name, type]: args) {
		arguments.push_back(argument_name);
		VariablePtr argument = Variable::make(argument_name, type, *this);
		argument->init();
		argumentMap.emplace(argument_name, argument);
		if (i < Why::argumentCount) {
			argument->reg = Why::argumentOffset + i;
		} else
			throw LocatedError(getLocation(), "Functions with greater than " + std::to_string(Why::argumentCount) +
				" arguments are currently unsupported.");
		if (selfScope->doesConflict(argument_name))
			throw NameConflictError(argument_name);
		variables.emplace(argument_name, argument);
		++i;
	}
}

size_t Function::argumentCount() const {
	if (structParent && argumentMap.count("this") != 0)
		return arguments.size() - 1;
	return arguments.size();
}

void Function::compile() {
	const bool is_init = name == ".init";

	if (!is_init) {
		if (isBuiltin())
			return;

		if (!source)
			throw LocatedError(getLocation(), "Can't compile " + name + ": no source node");

		if (source->size() == 5) {
			const std::string &struct_name = *source->at(4)->text;
			if (program.structs.count(struct_name) == 0)
				throw LocatedError(getLocation(), "Couldn't find struct " + struct_name + " for function " + struct_name
					+ "::" + name);
			if (argumentMap.count("this") != 0)
				throw LocatedError(getLocation(), "Scope method " + struct_name + "::" + name + " cannot take a "
					"parameter named \"this\"");
			std::vector<std::string> new_arguments {"this"};
			new_arguments.insert(new_arguments.end(), arguments.cbegin(), arguments.cend());
			arguments = std::move(new_arguments);
			structParent = program.structs.at(struct_name);
			auto this_var = Variable::make("this", PointerType::make(structParent->copy()), *this);
			this_var->init();
			argumentMap.emplace("this", this_var);
			variables.emplace("this", this_var);
		} else if (source->size() != 4)
			throw LocatedError(getLocation(), "Expected 4 nodes in " + name + "'s source node, found " +
				std::to_string(source->size()));

		for (const ASTNode *child: *source->at(2))
			switch (child->symbol) {
				case CMMTOK_NAKED:
					attributes.insert(Attribute::Naked);
					break;
				default:
					throw LocatedError(getLocation(), "Invalid fnattr: " + *child->text);
			}

		if (!isNaked()) {
			int i = 0;
			VregPtr temp_var = arguments.empty()? nullptr : newVar();
			VregPtr fp = precolored(Why::framePointerOffset);
			for (const std::string &argument_name: arguments) {
				VariablePtr argument = argumentMap.at(argument_name);
				const size_t offset = addToStack(argument);
				add<MoveInstruction>(precolored(Why::argumentOffset + i++), argument);
				add<SubIInstruction>(fp, temp_var, offset);
				add<StoreRInstruction>(argument, temp_var, argument->getSize());
			}
		}

		for (const ASTNode *child: *source->at(3))
			compile(*child);
	}

	if (!isNaked()) {
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
			if (stackUsage != 0)
				addFront<SubIInstruction>(sp, sp, stackUsage);
			addFront<MoveInstruction>(sp, fp);
			for (int reg: gp_regs)
				addFront<StackPushInstruction>(precolored(reg));
			addFront<StackPushInstruction>(fp);
			addFront<StackPushInstruction>(rt);
			add<Label>("." + mangle() + ".e");
			add<MoveInstruction>(fp, sp);
			for (int reg: gp_regs)
				add<StackPopInstruction>(precolored(reg));
			add<StackPopInstruction>(fp);
			add<StackPopInstruction>(rt);
		}

		add<JumpRegisterInstruction>(rt, false);
	}
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

VregPtr Function::newVar(TypePtr type) {
	return std::make_shared<VirtualRegister>(*this, type)->init();
}

ScopePtr Function::newScope(int *id_out) {
	const int new_id = ++nextScope;
	auto new_scope = std::make_shared<BlockScope>(currentScope());
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
		throw LocatedError(getLocation(), "Variable already on the stack in function " + name + ": " + variable->name);
	stackOffsets.emplace(variable, stackUsage += variable->type->getSize());
	return stackUsage;
}

void Function::compile(const ASTNode &node, const std::string &break_label, const std::string &continue_label) {
	switch (node.symbol) {
		case CMM_DECL: {
			checkNaked(node);
			const std::string &var_name = *node.at(1)->text;
			if (currentScope()->doesConflict(var_name))
				throw NameConflictError(var_name, node.at(1)->location);
			VariablePtr variable = Variable::make(var_name, TypePtr(Type::get(*node.at(0), program)), *this);
			variable->init();
			currentScope()->insert(variable);
			size_t offset = addToStack(variable);
			if (node.size() == 3) {
				auto expr = ExprPtr(Expr::get(*node.at(2), this));
				auto fp = precolored(Why::framePointerOffset);
				if (auto *initializer = expr->cast<InitializerExpr>()) {
					auto addr_var = newVar();
					add<SubIInstruction>(fp, addr_var, offset);
					initializer->fullCompile(addr_var, *this, currentScope());
				} else {
					expr->compile(variable, *this, currentScope());
					typeCheck(*expr->getType(currentScope()), *variable->type, variable, *this);
					if (offset == 0) {
						add<StoreRInstruction>(variable, fp, variable->getSize());
					} else {
						VregPtr m0 = mx(0);
						add<SubIInstruction>(fp, m0, offset);
						add<StoreRInstruction>(variable, m0, variable->getSize());
					}
				}
			}
			break;
		}
		case CMMTOK_RETURN: {
			checkNaked(node);
			if (node.empty()) {
				if (!returnType->isVoid())
					throw LocatedError(getLocation(), "Must return an expression in non-void function " + name);
			} else {
				auto expr = ExprPtr(Expr::get(*node.front(), this));
				auto r0 = precolored(Why::returnValueOffset);
				expr->compile(r0, *this, currentScope());
				auto expr_type = expr->getType(currentScope());
				typeCheck(*expr_type, *returnType, r0, *this, node.location);
			}
			add<JumpInstruction>("." + mangle() + ".e");
			break;
		}
		case CMMTOK_LPAREN:
			checkNaked(node);
			ExprPtr(Expr::get(node, this))->compile(nullptr, *this, currentScope());
			break;
		case CMMTOK_WHILE: {
			checkNaked(node);
			ExprPtr condition = ExprPtr(Expr::get(*node.front(), this));
			const std::string label = "." + mangle() + "." + std::to_string(++nextBlock);
			const std::string start = label + "w.s", end = label + "w.e";
			add<Label>(start);
			auto temp_var = newVar();
			const TypePtr condition_type = condition->getType(currentScope());
			if (!(*condition_type && BoolType()))
				throw ImplicitConversionError(condition_type, BoolType::make(), condition->location);
			condition->compile(temp_var, *this, currentScope());
			add<LnotRInstruction>(temp_var, temp_var);
			add<JumpConditionalInstruction>(end, temp_var);
			auto scope = newScope();
			scopeStack.push_back(scope);
			compile(*node.at(1), end, start);
			scopeStack.pop_back();
			add<JumpInstruction>(start);
			add<Label>(end);
			break;
		}
		case CMMTOK_FOR: {
			checkNaked(node);
			auto scope = newScope();
			scopeStack.push_back(scope);

			const std::string label = "." + mangle() + "." + std::to_string(++nextBlock);
			const std::string start = label + "f.s", end = label + "f.e", next = label + "f.n";
			auto temp_var = newVar();

			compile(*node.front());
			add<Label>(start);
			ExprPtr condition = ExprPtr(Expr::get(*node.at(1), this));
			const TypePtr condition_type = condition->getType(currentScope());
			if (!(*condition_type && BoolType()))
				throw ImplicitConversionError(condition_type, BoolType::make(), condition->location);
			condition->compile(temp_var, *this, currentScope());
			add<LnotRInstruction>(temp_var, temp_var);
			add<JumpConditionalInstruction>(end, temp_var);
			compile(*node.at(3), end, next);
			add<Label>(next);
			compile(*node.at(2));
			add<JumpInstruction>(start);
			add<Label>(end);
			scopeStack.pop_back();
			break;
		}
		case CMMTOK_CONTINUE:
			checkNaked(node);
			if (continue_label.empty())
				throw LocatedError(node.location, "Encountered invalid continue statement");
			add<JumpInstruction>(continue_label);
			break;
		case CMMTOK_BREAK:
			checkNaked(node);
			if (break_label.empty())
				throw LocatedError(node.location, "Encountered invalid break statement");
			add<JumpInstruction>(break_label);
			break;
		case CMM_EMPTY:
			break;
		case CMM_BLOCK:
			checkNaked(node);
			for (const ASTNode *child: node)
				compile(*child, break_label, continue_label);
			break;
		case CMMTOK_IF: {
			checkNaked(node);
			const std::string base = "." + mangle() + "." + std::to_string(++nextBlock), end_label = base + "if.end";
			ExprPtr condition = ExprPtr(Expr::get(*node.front(), this));
			auto temp_var = newVar();
			const TypePtr condition_type = condition->getType(currentScope());
			if (!(*condition_type && BoolType()))
				throw ImplicitConversionError(condition_type, BoolType::make(), condition->location);
			condition->compile(temp_var, *this, currentScope());
			add<LnotRInstruction>(temp_var, temp_var);
			if (node.size() == 3) {
				const std::string else_label = base + "if.else";
				add<JumpConditionalInstruction>(else_label, temp_var);
				auto scope = newScope();
				scopeStack.push_back(scope);
				compile(*node.at(1), break_label, continue_label);
				scopeStack.pop_back();
				add<JumpInstruction>(end_label);
				add<Label>(else_label);
				scope = newScope();
				scopeStack.push_back(scope);
				compile(*node.at(2), break_label, continue_label);
				scopeStack.pop_back();
			} else {
				add<JumpConditionalInstruction>(end_label, temp_var);
				auto scope = newScope();
				scopeStack.push_back(scope);
				compile(*node.at(1), break_label, continue_label);
				scopeStack.pop_back();
			}
			add<Label>(end_label);
			break;
		}
		case CMMTOK_ASM: {
			wasmParser.errorCount = 0;
			const std::string wasm_source = node.front()->unquote();
			wasmLexer.location = {0, 0};
			wasmParser.in(wasm_source);
			wasmParser.debug(false, false);
			wasmParser.parse();
			if (wasmParser.errorCount != 0 || wasmLexer.failed) {
				std::cerr << "\e[31mWASM parsing failed for ASM node at " << node.location << "\e[39m\n";
				std::cerr << "\e[31mFull text: [\e[1m" << wasm_source << "\e[22m]\e[39m\n";
			} else {
				const ASTNode *input_exprs  = 2 <= node.size()? node.at(1) : nullptr,
				              *output_exprs = 3 <= node.size()? node.at(2) : nullptr;
				const size_t  input_count  = input_exprs?  input_exprs->size()  : 0;
				const size_t  output_count = output_exprs? output_exprs->size() : 0;

				if (isNaked() && (input_count != 0 || output_count != 0))
					throw LocatedError(node.location,
						"Can't supply inputs or outputs to asm nodes in a naked function");

				VarMap map;
				std::map<std::string, ExprPtr> out_exprs;

				if (output_exprs) {
					size_t output_counter = 0;
					for (const ASTNode *child: *output_exprs) {
						auto expr = ExprPtr(Expr::get(*child, this));
						const std::string wasm_vreg = "$" + std::to_string(1 + input_count + output_counter++);
						map.emplace(wasm_vreg, newVar());
						out_exprs.emplace(wasm_vreg, expr);
					}
				}

				if (input_exprs) {
					size_t input_counter = 0;
					for (const ASTNode *child: *input_exprs) {
						auto expr = ExprPtr(Expr::get(*child, this));
						const std::string wasm_vreg = "$" + std::to_string(1 + input_counter++);
						VregPtr var = newVar();
						expr->compile(var, *this, currentScope());
						map.emplace(wasm_vreg, var);
					}
				}

				for (ASTNode *child: *wasmParser.root)
					if (auto *wasm_node = dynamic_cast<WASMInstructionNode *>(child)) {
						WhyPtr converted = wasm_node->convert(*this, map);
						instructions.push_back(converted);
					}

				if (!out_exprs.empty()) {
					auto addr_var = newVar();
					auto scope = currentScope();
					for (const auto &[wasm_vreg, expr]: out_exprs) {
						if (!expr->compileAddress(addr_var, *this, scope))
							throw LvalueError(std::string(*expr->getType(scope)));
						add<StoreRInstruction>(map.at(wasm_vreg), addr_var, expr->getSize(scope));
					}
				}
			}
			wasmParser.done();
			break;
		}
		case CMM_CAST:
		case CMMTOK_ASSIGN:
		case CMMTOK_PLUSPLUS:
		case CMMTOK_MINUSMINUS:
		case CMM_POSTPLUS:
		case CMM_POSTMINUS:
		case CMMTOK_PLUSEQ:
		case CMMTOK_MINUSEQ:
		case CMMTOK_DIVEQ:
		case CMMTOK_TIMESEQ:
		case CMMTOK_MODEQ:
		case CMMTOK_SREQ:
		case CMMTOK_SLEQ:
		case CMMTOK_ANDEQ:
		case CMMTOK_OREQ:
		case CMMTOK_XOREQ: {
			ExprPtr(Expr::get(node, this))->compile(nullptr, *this, currentScope());
			break;
		}
		default:
			warn() << "Unhandled node in Function::compile:\n";
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
				current = BasicBlock::make(*this, "." + mangle() + ".anon." + std::to_string(anons++));
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
				BasicBlockPtr new_block = BasicBlock::make(*this, "." + mangle() + ".anon." + std::to_string(anons++));

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
		throw LocatedError(getLocation(), "Can't spill vreg " + vreg->regOrID() + " in function " + name +
			": no definitions");
	}

	bool out = false;
	const size_t location = getSpill(vreg, true);

	for (std::weak_ptr<WhyInstruction> weak_definition: vreg->writers) {
		WhyPtr definition = weak_definition.lock();
		auto store = std::make_shared<StackStoreInstruction>(vreg, location);
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
		spilledVregs.insert(variable);
		// TODO: verify (see ff5e26da04a0d8846f026ba6c4e31370f3511110)
		return spillLocations[variable] = stackUsage += Why::wordSize;
	}
	throw LocatedError(getLocation(), "Couldn't find a spill location for " + variable->regOrID());
}

void Function::markSpilled(VregPtr vreg) {
	spilledVregs.insert(vreg);
}

bool Function::isSpilled(VregPtr vreg) const {
	return spilledVregs.count(vreg) != 0;
}

bool Function::canSpill(VregPtr vreg) {
	if (vreg->writers.empty() || isSpilled(vreg))
		return false;

	// If the only definition is a stack store, the variable can't be spilled.
	if (vreg->writers.size() == 1) {
		auto single_def = vreg->writers.begin()->lock();
		if (!single_def) {
			warn() << *vreg << '\n';
			throw LocatedError(getLocation(), "Can't lock single writer of instruction");
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
		auto store = std::make_shared<StackStoreInstruction>(vreg, location);
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
		throw LocatedError(getLocation(), "Couldn't lock instruction's parent block");
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
		throw LocatedError(getLocation(), "Couldn't lock instruction's parent block");
	}

	if (&block->function != this)
		throw LocatedError(getLocation(), "Block parent isn't equal to this in Function::insertBefore");

	// if (new_instruction->debugIndex == -1)
	// 	new_instruction->debugIndex = base->debugIndex;

	new_instruction->parent = base->parent;

	auto instructionIter = std::find(instructions.begin(), instructions.end(), base);
	if (linear_warn && instructionIter == instructions.end()) {
		warn() << "Couldn't find instruction in instructions field of function " << name << ": " << *base << '\n';
		throw LocatedError(getLocation(), "Couldn't find instruction in instructions field of function " + name);
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
		throw LocatedError(getLocation(), "Instruction not found in block");
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

	if (blocks.empty())
		return cfg;

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
				throw LocatedError(getLocation(), "Couldn't lock predecessor of " + label);
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

bool Function::isNaked() const {
	return attributes.count(Attribute::Naked) != 0;
}

void Function::checkNaked(const ASTNode &node) const {
	if (isNaked()) {
		node.debug();
		throw LocatedError(node.location, "Can't translate node in a naked function");
	}
}

void Function::doPointerArithmetic(TypePtr left_type, TypePtr right_type, Expr &left, Expr &right, VregPtr left_var,
                                   VregPtr right_var, ScopePtr scope, const ASTLocation &location) {
	if (left_type->isPointer() && right_type->isInt()) {
		auto *left_subtype = dynamic_cast<PointerType &>(*left_type).subtype;
		left.compile(left_var, *this, scope, 1);
		right.compile(right_var, *this, scope, left_subtype->getSize());
	} else if (left_type->isInt() && right_type->isPointer()) {
		auto *right_subtype = dynamic_cast<PointerType &>(*right_type).subtype;
		left.compile(left_var, *this, scope, right_subtype->getSize());
		right.compile(right_var, *this, scope, 1);
	} else if (!(*left_type && *right_type)) {
		throw ImplicitConversionError(TypePtr(left_type->copy()), TypePtr(right_type->copy()), location);
	} else {
		left.compile(left_var, *this, scope);
		right.compile(right_var, *this, scope);
	}
}

Function * Function::demangle(const std::string &mangled, Program &program) {
	// TODO: insert "this" argument as appropriate

	if (mangled.size() < 3 || (mangled.front() != '_' && mangled.front() != '.'))
		throw std::runtime_error("Invalid mangled function: \"" + mangled + "\"");
	const char *c_str = mangled.c_str();

	std::string struct_name;

	if (*c_str == '.') {
		size_t struct_name_size = 0;
		for (++c_str; std::isdigit(*c_str); ++c_str)
			struct_name_size = struct_name_size * 10 + (*c_str - '0');
		struct_name = std::string(c_str, struct_name_size);
	} else
		++c_str;

	size_t name_size = 0;
	for (; std::isdigit(*c_str); ++c_str)
		name_size = name_size * 10 + (*c_str - '0');
	const std::string name(c_str, name_size);
	c_str += name_size;
	size_t argument_count = 0;
	auto return_type = TypePtr(Type::get(c_str, program));
	for (; std::isdigit(*c_str); ++c_str)
		argument_count = argument_count * 10 + (*c_str - '0');
	std::vector<TypePtr> argument_types;
	std::vector<std::string> argument_names;
	for (size_t i = 0; i < argument_count; ++i) {
		argument_types.emplace_back(Type::get(c_str, program));
		argument_names.emplace_back("a" + std::to_string(i));
	}

	Function *out = nullptr;
	try {
		out = new Function(program, nullptr);
		out->name = name;
		out->returnType = return_type;
		out->arguments = std::move(argument_names);
		for (size_t i = 0; i < argument_count; ++i) {
			const std::string &argument_name = out->arguments.at(i);
			out->argumentMap.emplace(argument_name, Variable::make(argument_name, argument_types.at(i), *out));
		}
		if (!struct_name.empty()) {
			if (program.structs.count(struct_name) == 0)
				throw std::runtime_error("Error demangling function " + struct_name + "::" + name + ": struct " +
					struct_name + " not defined");
			out->structParent = program.structs.at(struct_name);
		}
	} catch (...) {
		delete out;
		throw;
	}

	return out;
}

bool Function::isDeclaredOnly() const {
	return !source && !isBuiltin();
}

bool Function::isMatch(TypePtr return_type, const std::vector<TypePtr> &argument_types, const std::string &struct_name)
const {
	if (return_type && *returnType != *return_type)
		return false;

	if (structParent) {
		const size_t count = argumentCount();

		if (structParent->name != struct_name || argument_types.size() != count)
			return false;

		size_t offset = count == arguments.size()? 0 : 1;
		for (size_t i = offset; i < count; ++i)
			if (!(*argument_types.at(i - offset) && *argumentMap.at(arguments.at(i))->type))
				return false;

		return true;
	}

	if (argument_types.size() != arguments.size())
		return false;

	for (size_t i = 0, max = arguments.size(); i < max; ++i)
		if (!(*argument_types.at(i) && *argumentMap.at(arguments.at(i))->type))
			return false;

	return struct_name.empty();
}
