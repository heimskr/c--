#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <optional>

#include "ColoringAllocator.h"
#include "Function.h"
#include "WhyInstructions.h"
#include "Variable.h"
#include "Errors.h"
// #include "pass/MakeCFG.h"
// #include "pass/SplitBlocks.h"
// #include "util/Timer.h"
#include "Util.h"
#include "Why.h"

#define CONSTRUCT_BY_BLOCK
#define SELECT_MOST_LIVE

ColoringAllocator::Result ColoringAllocator::attempt() {
	++attempts;
#ifdef DEBUG_COLORING
	std::cerr << "Allocating for \e[1m" << *function.name << "\e[22m.\n";
#endif

	makeInterferenceGraph();
	try {
		interference.color(Graph::ColoringAlgorithm::Greedy, Why::temporaryOffset,
			Why::savedOffset + Why::savedCount - 1);
	} catch (const UncolorableError &err) {
#ifdef DEBUG_COLORING
		std::cerr << "Coloring failed.\n";
#endif
		VregPtr to_spill = selectMostLive();

		if (!to_spill) {
			error() << "to_spill is null!\n";
			throw std::runtime_error("to_spill is null");
		}
#ifdef DEBUG_COLORING
		std::cerr << "Going to spill " << *to_spill;
#if !defined(SELECT_LOWEST_COST) && !defined(SELECT_MOST_LIVE) && !defined(SELECT_CHAITIN)
		std::cerr << " (degree: " << highest_degree << ")";
#endif
		std::cerr << ". Likely name: " << function.variableStore.size() << "\n";
		std::cerr << "Can spill: " << std::boolalpha << function.canSpill(to_spill) << "\n";
#endif
		triedIDs.insert(to_spill->id);
		// triedLabels.insert(*to_spill->id);
#ifdef DEBUG_COLORING
		info() << "Variable to spill: " << *to_spill << " (ID: " << to_spill->id << ")\n";
#endif
		lastSpillAttempt = to_spill;
		triedIDs.insert(to_spill->id);
		// triedLabels.insert(*to_spill->id);

		if (function.spill(to_spill)) {
#ifdef DEBUG_COLORING
			std::cerr << "Spilled " << *to_spill << ". Variables: " << function.variableStore.size()
						<< ". Stack locations: " << function.stack.size() << "\n";
#endif
			lastSpill = to_spill;
			++spillCount;
			// int split = Passes::splitBlocks(*function);
			int split = function.split();
			if (0 < split) {
#ifdef DEBUG_COLORING
				std::cerr << split << " block" << (split == 1? " was" : "s were") << " split.\n";
#endif
				for (BasicBlockPtr &block: function.blocks)
					// block->extract();
					block->cacheReadWritten();
				// Passes::makeCFG(*function);
				// function.makeCFG();
				// function.extractVariables(true);
				function.computeLiveness();
			}
#ifdef DEBUG_COLORING
			else std::cerr << "No blocks were split.\n";
#endif
			return Result::Spilled;
		}
#ifdef DEBUG_COLORING
		else std::cerr << *to_spill << " wasn't spilled.\n";
#endif
		return Result::NotSpilled;
	}

#ifdef DEBUG_COLORING
	std::cerr << "Spilling process complete. There " << (spillCount == 1? "was " : "were ") << spillCount << " "
				<< "spill" << (spillCount == 1? ".\n" : "s.\n");
#endif

	for (const std::pair<const std::string, Node *> &pair: interference) {
		VregPtr ptr = pair.second->get<VregPtr>();
#ifdef DEBUG_COLORING
		std::cerr << "Variable " << std::string(*ptr) << ": " << ptr->registersString() << " -> ( ";
		for (const int color: pair.second->colors) std::cerr << color << ' ';
		std::cerr << ") a =";
		for (const Variable *alias: ptr->getAliases())
			std::cerr << ' ' << *alias;
		std::cerr << '\n';
#endif
		if (ptr->reg == -1)
			ptr->reg == *pair.second->colors.begin();
	}

	return Result::Success;
}

VregPtr ColoringAllocator::selectHighestDegree(int *degree_out) const {
	const Node *highest_node = nullptr;
	int highest = -1;
	// std::cerr << "Avoid["; for (const std::string &s: triedLabels) std::cerr << " " << s; std::cerr << " ]\n";
	for (const Node *node: interference.nodes()) {
		const int degree = node->degree();
		// if (highest < degree && triedLabels.count(node->label()) == 0) {
		// if (highest < degree && function.canSpill(node->get<VregPtr>())) {
		if (highest < degree && triedLabels.count(node->label()) == 0
				&& function.canSpill(node->get<VregPtr>())) {
			highest_node = node;
			highest = degree;
		}
	}

	if (!highest_node)
		throw NoChoiceError("Couldn't find node with highest degree out of " +
			std::to_string(interference.nodes().size()) + " node(s)");

	std::vector<const Node *> all_highest;
	for (const Node *node: interference.nodes())
		if (node->degree() == static_cast<size_t>(highest))
			all_highest.push_back(node);

	if (degree_out)
		*degree_out = highest;

	return highest_node->get<VregPtr>();
}

VregPtr ColoringAllocator::selectMostLive(int *liveness_out) const {
	// Timer timer("SelectMostLive");
	VregPtr ptr;
	int highest = -1;
	for (const auto &var: function.virtualRegisters) {
		if (Why::isSpecialPurpose(var->reg) || !function.canSpill(var))
			continue;
		const int sum = function.getLiveIn(var).size() + function.getLiveOut(var).size();
		if (highest < sum && triedIDs.count(var->originalID) == 0) {
			highest = sum;
			ptr = var;
		}
	}

	if (!ptr) {
		// function.debug();
		throw std::runtime_error("Couldn't select variable with highest liveness");
	}

	if (liveness_out)
		*liveness_out = highest;

	if (!function.canSpill(ptr))
		warn() << "Impossibility detected: can't spill " << *ptr << "\n";

	return ptr;
}

VregPtr ColoringAllocator::selectChaitin() const {
	VregPtr out;
	long lowest = LONG_MAX;
	for (const Node *node: interference.nodes()) {
		VregPtr var = node->get<VregPtr>();
		if (var->allRegistersSpecial() || !function.canSpill(var))
			continue;
		var->clearSpillCost();
		const int cost = var->spillCost();
		if (cost == INT_MAX)
			continue;
		const int degree = node->degree();
		const long chaitin = cost * 10000L / degree;
		if (chaitin < lowest) {
			lowest = chaitin;
			out = var;
		}
	}

	if (!out)
		throw NoChoiceError("Couldn't select a variable in ColoringAllocator::selectChaitin.");

	return out;
}

// #undef DEBUG_COLORING

void ColoringAllocator::makeInterferenceGraph() {
	// Timer timer("MakeInterferenceGraph");
	interference.clear();
	size_t links = 0;

	for (const auto &[id, var]: function.variables) {
#ifdef DEBUG_COLORING
		// std::cerr << "%% " << pair.first << " " << *pair.second << "; aliases:";
		// for (Variable *v: pair.second->getAliases()) std::cerr << " " << *v;
		// std::cerr << "\n";
#endif
		if (var->reg == -1) {
			// const std::string *parent_id = var->parentID();
			const std::string id = std::to_string(var->id);
			if (!interference.hasLabel(id)) { // Use only one variable from a set of aliases.
				Node &node = interference.addNode(id);
				node.data = var;
#ifdef DEBUG_COLORING
				// info() << *var << ": " << var->registersRequired() << " required.";
				// if (var->type)
				// 	std::cerr << " " << std::string(*var->type);
				// std::cerr << "\n";
#endif
				// node.colorsNeeded = var->registersRequired();
				node.colorsNeeded = 1;
#ifdef DEBUG_COLORING
			} else {
				// std::cerr << "Skipping " << *var << " (" << *id << "): parent (" << *parent_id << ") is in graph\n";
#endif
			}
		}
	}

	std::vector<std::string> labels;
	labels.reserve(function.virtualRegisters.size());
	for (const auto &var: function.virtualRegisters)
		labels.push_back(std::to_string(var->id));

	std::unordered_map<int, std::vector<int>> vecs;
	std::unordered_map<int, std::set<int>> sets;

	for (const auto &var: function.virtualRegisters) {
		const int id = var->id;
		if (var->reg != -1)
			continue;
		for (const std::weak_ptr<BasicBlock> &bptr: var->definingBlocks) {
			const auto index = bptr.lock()->index;
			if (sets[index].count(id) == 0) {
				vecs[index].push_back(id);
				sets[index].insert(id);
			}
		}
		for (const std::weak_ptr<BasicBlock> &bptr: var->usingBlocks) {
			const auto index = bptr.lock()->index;
			if (sets[index].count(id) == 0) {
				vecs[index].push_back(id);
				sets[index].insert(id);
			}
		}
	}

	for (const std::shared_ptr<BasicBlock> &block: function.blocks) {
		auto &vec = vecs[block->index];
		auto &set = sets[block->index];
		for (const VregPtr &var: block->liveIn) {
			const int id = var->id;
			if (var->reg == -1 && set.count(id) == 0) {
				vec.push_back(id);
				set.insert(id);
			}
		}
		for (const VregPtr &var: block->liveOut) {
			const int id = var->id;
			if (var->reg == -1 && set.count(id) == 0) {
				vec.push_back(id);
				set.insert(id);
			}
		}
	}

#ifdef DEBUG_COLORING
	std::cerr << "Label count: " << labels.size() << "\n";
#endif
	for (const auto &[block_id, vec]: vecs) {
		const size_t size = vec.size();
		if (size < 2)
			continue;
		for (size_t i = 0; i < size - 1; ++i)
			for (size_t j = i + 1; j < size; ++j) {
				interference.link(std::to_string(vec[i]), std::to_string(vec[j]), true);
				++links;
			}
	}

#ifdef DEBUG_COLORING
	std::cerr << "Made " << links << " link" << (links == 1? "" : "s") << ".\n";
#endif
}
