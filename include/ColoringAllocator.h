#pragma once

#include <memory>
#include <set>

#include "Allocator.h"
#include "Variable.h"
#include "Graph.h"

/** Assigns registers using a graph coloring algorithm. */
class ColoringAllocator: public Allocator {
	private:
		std::set<int> triedIDs;
		// std::set<std::string> triedLabels;

	public:
		Graph interference = Graph("interference");

		using Allocator::Allocator;

		/** Creates an interference graph of all the function's variables. */
		void makeInterferenceGraph();

		/** Selects the variable whose corresponding node in the interference graph has the highest degree. */
		std::shared_ptr<VirtualRegister> selectHighestDegree(int *degree_out = nullptr) const;

		/** Selects the variable with the lowest spill cost. */
		std::shared_ptr<VirtualRegister> selectLowestSpillCost() const;

		std::shared_ptr<VirtualRegister> selectMostLive(int *liveness_out = nullptr) const;

		std::shared_ptr<VirtualRegister> selectChaitin() const;

		/** Makes an attempt to allocate registers. If the graph is uncolorable, the function attempts to spill a
		 *  variable. If one was spilled, it returns Spilled; otherwise, it returns NotSpilled. If the graph was
		 *  colorable, it returns Success. */
		Result attempt() override;

		const decltype(triedIDs)    & getTriedIDs()    const { return triedIDs;    }
		const decltype(triedLabels) & getTriedLabels() const { return triedLabels; }
};
