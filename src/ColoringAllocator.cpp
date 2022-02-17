// #include <algorithm>
// #include <climits>
// #include <fstream>
// #include <iostream>
// #include <optional>

// #include "ColoringAllocator.h"
// #include "Function.h"
// #include "WhyInstructions.h"
// #include "Variable.h"
// #include "Errors.h"
// // #include "pass/MakeCFG.h"
// // #include "pass/SplitBlocks.h"
// // #include "util/Timer.h"
// #include "Util.h"
// #include "Why.h"

// #define CONSTRUCT_BY_BLOCK
// #define SELECT_MOST_LIVE

// ColoringAllocator::Result ColoringAllocator::attempt() {
// 	++attempts;
// #ifdef DEBUG_COLORING
// 	std::cerr << "Allocating for \e[1m" << *function.name << "\e[22m.\n";
// #endif

// 	makeInterferenceGraph();
// 	try {
// 		interference.color(Graph::ColoringAlgorithm::Greedy, Why::temporaryOffset,
// 			Why::savedOffset + Why::savedCount - 1);
// 	} catch (const UncolorableError &err) {
// #ifdef DEBUG_COLORING
// 		std::cerr << "Coloring failed.\n";
// #endif
// 		int highest_degree = -1;
// #ifdef SELECT_LOWEST_COST
// 		VregPtr to_spill = selectLowestSpillCost();
// 		(void) highest_degree;
// #elif defined(SELECT_CHAITIN)
// 		VregPtr to_spill = selectChaitin();
// 		(void) highest_degree;
// #elif defined(SELECT_MOST_LIVE)
// 		VregPtr to_spill = selectMostLive();
// 		(void) highest_degree;
// #else
// 		VregPtr to_spill = selectHighestDegree(&highest_degree);
// 		if (highest_degree == -1)
// 			throw std::runtime_error("highest_degree is -1");
// #endif

// 		if (!to_spill) {
// 			error() << "to_spill is null!\n";
// 			throw std::runtime_error("to_spill is null");
// 		}
// #ifdef DEBUG_COLORING
// 		std::cerr << "Going to spill " << *to_spill;
// #if !defined(SELECT_LOWEST_COST) && !defined(SELECT_MOST_LIVE) && !defined(SELECT_CHAITIN)
// 		std::cerr << " (degree: " << highest_degree << ")";
// #endif
// 		std::cerr << ". Likely name: " << function.variableStore.size() << "\n";
// 		std::cerr << "Can spill: " << std::boolalpha << function.canSpill(to_spill) << "\n";
// #endif
// 		triedIDs.insert(to_spill->id);
// 		// triedLabels.insert(*to_spill->id);
// #ifdef DEBUG_COLORING
// 		info() << "Variable to spill: " << *to_spill << " (ID: " << to_spill->id << ")\n";
// #endif
// 		lastSpillAttempt = to_spill;
// 		triedIDs.insert(to_spill->id);
// 		// triedLabels.insert(*to_spill->id);

// 		if (function.spill(to_spill)) {
// #ifdef DEBUG_COLORING
// 			std::cerr << "Spilled " << *to_spill << ". Variables: " << function.variableStore.size()
// 						<< ". Stack locations: " << function.stack.size() << "\n";
// #endif
// 			lastSpill = to_spill;
// 			++spillCount;
// 			// int split = Passes::splitBlocks(*function);
// 			int split = function.split();
// 			if (0 < split) {
// #ifdef DEBUG_COLORING
// 				std::cerr << split << " block" << (split == 1? " was" : "s were") << " split.\n";
// #endif
// 				for (BasicBlockPtr &block: function.blocks)
// 					// block->extract();
// 					block->cacheReadWritten();
// 				// Passes::makeCFG(*function);
// 				function.makeCFG();
// 				// function.extractVariables(true);
// 				function.computeLiveness();
// 			}
// #ifdef DEBUG_COLORING
// 			else std::cerr << "No blocks were split.\n";
// #endif
// 			return Result::Spilled;
// 		}
// #ifdef DEBUG_COLORING
// 		else std::cerr << *to_spill << " wasn't spilled.\n";
// #endif
// 		return Result::NotSpilled;
// 	}

// #ifdef DEBUG_COLORING
// 	std::cerr << "Spilling process complete. There " << (spillCount == 1? "was " : "were ") << spillCount << " "
// 				<< "spill" << (spillCount == 1? ".\n" : "s.\n");
// #endif

// 	for (const std::pair<const std::string, Node *> &pair: interference) {
// 		VregPtr ptr = pair.second->get<VregPtr>();
// #ifdef DEBUG_COLORING
// 		std::cerr << "Variable " << std::string(*ptr) << ": " << ptr->registersString() << " -> ( ";
// 		for (const int color: pair.second->colors) std::cerr << color << ' ';
// 		std::cerr << ") a =";
// 		for (const Variable *alias: ptr->getAliases())
// 			std::cerr << ' ' << *alias;
// 		std::cerr << '\n';
// #endif
// 		if (ptr->registers.empty()) {
// 			std::set<int> assigned;
// 			for (const int color: pair.second->colors)
// 				assigned.insert(color);
// 			ptr->setRegisters(assigned);
// 		}
// 	}

// 	return Result::Success;
// }

// VregPtr ColoringAllocator::selectHighestDegree(int *degree_out) const {
// 	const Node *highest_node = nullptr;
// 	int highest = -1;
// 	// std::cerr << "Avoid["; for (const std::string &s: triedLabels) std::cerr << " " << s; std::cerr << " ]\n";
// 	for (const Node *node: interference.nodes()) {
// 		const int degree = node->degree();
// 		// if (highest < degree && triedLabels.count(node->label()) == 0) {
// 		// if (highest < degree && function.canSpill(node->get<VregPtr>())) {
// 		if (highest < degree && triedLabels.count(node->label()) == 0
// 				&& function.canSpill(node->get<VregPtr>())) {
// 			highest_node = node;
// 			highest = degree;
// 		}
// 	}

// 	if (!highest_node)
// 		throw NoChoiceError("Couldn't find node with highest degree out of " +
// 			std::to_string(interference.nodes().size()) + " node(s)");

// 	std::vector<const Node *> all_highest;
// 	for (const Node *node: interference.nodes())
// 		if (node->degree() == static_cast<size_t>(highest))
// 			all_highest.push_back(node);

// 	if (degree_out)
// 		*degree_out = highest;

// 	return highest_node->get<VregPtr>();
// }

// VregPtr ColoringAllocator::selectLowestSpillCost() const {
// 	VregPtr ptr;
// 	int lowest = INT_MAX;
// 	for (const auto &[id, var]: function.variableStore) {
// 		if (var->allRegistersSpecial())
// 			continue;
// 		var->clearSpillCost();
// 		const int cost = var->spillCost();
// 		if (cost != -1 && triedIDs.count(var->id) == 0 && (cost < lowest && !var->isSimple())) {
// 			lowest = cost;
// 			ptr = var;
// 		}
// 	}

// 	if (!ptr)
// 		throw NoChoiceError("Couldn't select lowest spill cost variable");

// 	return ptr;
// }

// VregPtr ColoringAllocator::selectMostLive(int *liveness_out) const {
// 	// Timer timer("SelectMostLive");
// 	VregPtr ptr;
// 	int highest = -1;
// 	for (const auto *map: {&function.variableStore, &function.extraVariables})
// 		for (const auto &[id, var]: *map) {
// 			if (var->allRegistersSpecial() || !function.canSpill(var))
// 				continue;
// 			const int sum = function.getLiveIn(var).size() + function.getLiveOut(var).size();
// 			if (highest < sum && triedIDs.count(var->originalID) == 0) {
// 				highest = sum;
// 				ptr = var;
// 			}
// 		}

// 	if (!ptr) {
// 		// function.debug();
// 		throw std::runtime_error("Couldn't select variable with highest liveness");
// 	}

// 	if (liveness_out)
// 		*liveness_out = highest;

// 	if (!function.canSpill(ptr))
// 		warn() << "Impossibility detected: can't spill " << *ptr << "\n";

// 	return ptr;
// }

// VregPtr ColoringAllocator::selectChaitin() const {
// 	VregPtr out;
// 	long lowest = LONG_MAX;
// 	for (const Node *node: interference.nodes()) {
// 		VregPtr var = node->get<VregPtr>();
// 		if (var->allRegistersSpecial() || !function.canSpill(var))
// 			continue;
// 		var->clearSpillCost();
// 		const int cost = var->spillCost();
// 		if (cost == INT_MAX)
// 			continue;
// 		const int degree = node->degree();
// 		const long chaitin = cost * 10000L / degree;
// 		if (chaitin < lowest) {
// 			lowest = chaitin;
// 			out = var;
// 		}
// 	}

// 	if (!out)
// 		throw NoChoiceError("Couldn't select a variable in ColoringAllocator::selectChaitin.");

// 	return out;
// }

// // #undef DEBUG_COLORING

// void ColoringAllocator::makeInterferenceGraph() {
// 	// Timer timer("MakeInterferenceGraph");
// 	interference.clear();
// 	size_t links = 0;

// 	for (const auto &[id, var]: function.variableStore) {
// #ifdef DEBUG_COLORING
// 		// std::cerr << "%% " << pair.first << " " << *pair.second << "; aliases:";
// 		// for (Variable *v: pair.second->getAliases()) std::cerr << " " << *v;
// 		// std::cerr << "\n";
// #endif
// 		if (var->registers.empty()) {
// 			const std::string *parent_id = var->parentID();
// 			if (!interference.hasLabel(*parent_id)) { // Use only one variable from a set of aliases.
// 				Node &node = interference.addNode(*parent_id);
// 				node.data = var;
// #ifdef DEBUG_COLORING
// 				// info() << *var << ": " << var->registersRequired() << " required.";
// 				// if (var->type)
// 				// 	std::cerr << " " << std::string(*var->type);
// 				// std::cerr << "\n";
// #endif
// 				node.colorsNeeded = var->registersRequired();
// #ifdef DEBUG_COLORING
// 			} else {
// 				// std::cerr << "Skipping " << *var << " (" << *id << "): parent (" << *parent_id << ") is in graph\n";
// #endif
// 			}
// 		}
// 	}

// 	std::vector<Variable::ID> labels;
// 	labels.reserve(function.variableStore.size());
// 	for (const auto &[id, var]: function.variableStore)
// 		labels.push_back(id);

// #ifndef CONSTRUCT_BY_BLOCK
// 	// Maps a variable ID to a set of blocks in which the variable is live-in or live-out.
// 	std::map<Variable::ID, std::unordered_set<int>> live;

// 	for (const auto &[id, var]: function.variableStore) {
// 		if (!var->registers.empty())
// 			continue;
// #ifdef DEBUG_COLORING
// 		std::cerr << "Variable " << *var << ":\n";
// #endif
// 		for (const std::weak_ptr<BasicBlock> &bptr: var->definingBlocks) {
// 			live[var->id].insert(bptr.lock()->index);
// #ifdef DEBUG_COLORING
// 			std::cerr << "  definer: " << *bptr.lock()->label << " (" << bptr.lock()->index << ")\n";
// #endif
// 		}
// 		for (const std::weak_ptr<BasicBlock> &bptr: var->usingBlocks) {
// 			live[var->id].insert(bptr.lock()->index);
// #ifdef DEBUG_COLORING
// 			std::cerr << "  user: " << *bptr.lock()->label << " (" << bptr.lock()->index << ")\n";
// #endif
// 		}
// 	}

// 	for (const std::shared_ptr<BasicBlock> &block: function.blocks) {
// #ifdef DEBUG_COLORING
// 		if (!block)
// 			std::cerr << "block is null?\n";
// #endif
// 		for (const VregPtr &var: block->liveIn)
// 			if (var->registers.empty()) {
// #ifdef DEBUG_COLORING
// 				std::cerr << "Variable " << *var << " is live-in at block " << *block->label << "\n";
// #endif
// 				live[var->id].insert(block->index);
// 			}
// 		for (const VregPtr &var: block->liveOut)
// 			if (var->registers.empty()) {
// #ifdef DEBUG_COLORING
// 				std::cerr << "Variable " << *var << " is live-out at block " << *block->label << "\n";
// #endif
// 				live[var->id].insert(block->index);
// 			}
// 	}

// 	if (1 < labels.size()) {
// 		const size_t size = labels.size();
// #ifdef DEBUG_COLORING
// 		std::cerr << "Label count: " << size << "\n";
// #endif
// 		size_t checks = 0;
// 		for (size_t i = 0; i < size - 1; ++i) {
// 			for (size_t j = i + 1; j < size; ++j) {
// 				VregPtr left  = function.variableStore.at(labels[i]),
// 							right = function.variableStore.at(labels[j]);
// 				if (left->id != right->id && Util::hasOverlap(live[left->id], live[right->id])) {
// 					interference.link(*left->id, *right->id, true);
// 					++links;
// 				}
// 				++checks;
// 			}
// 		}
// #ifdef DEBUG_COLORING
// 		std::cerr << "Ran " << checks << " check" << (checks == 1? "" : "s") << ".\n";
// #endif
// 	}

// #else
// 	std::unordered_map<int, std::vector<Variable::ID>> vecs;
// 	std::unordered_map<int, std::unordered_set<Variable::ID>> sets;

// 	for (const auto &[id, var]: function.variableStore) {
// 		const Variable::ID parent_id = var->parentID();
// 		if (!var->registers.empty())
// 			continue;
// 		for (const std::weak_ptr<BasicBlock> &bptr: var->definingBlocks) {
// 			const auto index = bptr.lock()->index;
// 			if (sets[index].count(parent_id) == 0) {
// 				vecs[index].push_back(parent_id);
// 				sets[index].insert(parent_id);
// 			}
// 		}
// 		for (const std::weak_ptr<BasicBlock> &bptr: var->usingBlocks) {
// 			const auto index = bptr.lock()->index;
// 			if (sets[index].count(parent_id) == 0) {
// 				vecs[index].push_back(parent_id);
// 				sets[index].insert(parent_id);
// 			}
// 		}
// 	}

// 	for (const std::shared_ptr<BasicBlock> &block: function.blocks) {
// 		auto &vec = vecs[block->index];
// 		auto &set = sets[block->index];
// 		for (const VregPtr &var: block->liveIn) {
// 			const Variable::ID parent_id = var->parentID();
// 			if (var->registers.empty() && set.count(parent_id) == 0) {
// 				vec.push_back(parent_id);
// 				set.insert(parent_id);
// 			}
// 		}
// 		for (const VregPtr &var: block->liveOut) {
// 			const Variable::ID parent_id = var->parentID();
// 			if (var->registers.empty() && set.count(parent_id) == 0) {
// 				vec.push_back(parent_id);
// 				set.insert(parent_id);
// 			}
// 		}
// 	}

// #ifdef DEBUG_COLORING
// 	std::cerr << "Label count: " << labels.size() << "\n";
// #endif
// 	for (const auto &[block_id, vec]: vecs) {
// 		const size_t size = vec.size();
// 		if (size < 2)
// 			continue;
// 		for (size_t i = 0; i < size - 1; ++i)
// 			for (size_t j = i + 1; j < size; ++j) {
// 				interference.link(*vec[i], *vec[j], true);
// 				++links;
// 			}
// 	}
// #endif

// #ifdef DEBUG_COLORING
// 	std::cerr << "Made " << links << " link" << (links == 1? "" : "s") << ".\n";
// #endif
// }
