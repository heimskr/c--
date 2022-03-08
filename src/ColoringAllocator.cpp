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
#include "Util.h"
#include "Why.h"

ColoringAllocator::Result ColoringAllocator::attempt() {
	++attempts;
	makeInterferenceGraph();

	try {
		interference.color(Graph::ColoringAlgorithm::Greedy, Why::temporaryOffset,
			Why::savedOffset + Why::savedCount - 1);
	} catch (const UncolorableError &err) {
		VregPtr to_spill = selectMostLive();
		if (!to_spill)
			throw std::runtime_error("to_spill is null");
		triedIDs.insert(to_spill->id);
		lastSpillAttempt = to_spill;
		triedIDs.insert(to_spill->id);

		if (function.spill(to_spill)) {
			lastSpill = to_spill;
			++spillCount;
			if (0 < function.split()) {
				for (auto &block: function.blocks)
					block->cacheReadWritten();
				function.makeCFG();
				function.computeLiveness();
			}
			return Result::Spilled;
		}

		return Result::NotSpilled;
	}

	for (const std::pair<const std::string, Node *> &pair: interference) {
		VregPtr ptr = pair.second->get<VregPtr>();
		if (ptr->getReg() == -1)
			ptr->setReg(*pair.second->colors.begin());
	}

	return Result::Success;
}

VregPtr ColoringAllocator::selectMostLive(int *liveness_out) const {
	VregPtr ptr;
	int highest = -1;
	for (const auto &var: function.virtualRegisters) {
		if (Why::isSpecialPurpose(var->getReg()) || !function.canSpill(var))
			continue;

		const int sum = function.getLiveIn(var).size() + function.getLiveOut(var).size();
		if (highest < sum && triedIDs.count(var->id) == 0) {
			highest = sum;
			ptr = var;
		}
	}

	if (!ptr) {
		function.debug();
		throw std::runtime_error("Couldn't select variable with highest liveness");
	}

	if (liveness_out)
		*liveness_out = highest;

	if (!function.canSpill(ptr))
		warn() << "Impossibility detected: can't spill " << ptr->regOrID(true) << '\n';

	return ptr;
}

void ColoringAllocator::makeInterferenceGraph() {
	interference.clear();
	size_t links = 0;

	for (const auto &var: function.virtualRegisters) {
		if (var->getReg() == -1) {
			const std::string id = std::to_string(var->id);
			if (!interference.hasLabel(id)) {
				Node &node = interference.addNode(id);
				node.data = var;
				node.colorsNeeded = 1; // TODO: update if variables can span more than one register
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
		if (var->getReg() != -1)
			continue;
		for (const std::weak_ptr<BasicBlock> &bptr: var->writingBlocks) {
			const auto index = bptr.lock()->index;
			if (sets[index].count(id) == 0) {
				vecs[index].push_back(id);
				sets[index].insert(id);
			}
		}
		for (const std::weak_ptr<BasicBlock> &bptr: var->readingBlocks) {
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
			if (var->getReg() == -1 && set.count(id) == 0) {
				vec.push_back(id);
				set.insert(id);
			}
		}
		for (const VregPtr &var: block->liveOut) {
			const int id = var->id;
			if (var->getReg() == -1 && set.count(id) == 0) {
				vec.push_back(id);
				set.insert(id);
			}
		}
	}

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
}
