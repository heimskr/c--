#include "CFG.h"
#include "Function.h"
#include "Util.h"
#include "WhyInstructions.h"

Graph makeCFG(Function &function) {
	Graph cfg;
	cfg.name = "CFG for " + function.name;

	// First pass: add all the nodes.
	for (BasicBlockPtr &block: function.blocks) {
		const std::string &label = block->label;
		cfg += label;
		Node &node = cfg[label];
		node.data = std::weak_ptr<BasicBlock>(block);
		block->node = &node;
	}

	cfg += "exit";

	bool exit_linked = false;

	// Second pass: connect all the nodes.
	for (BasicBlockPtr &block: function.blocks) {
		const std::string &label = block->label;
		for (const auto &weak_pred: block->predecessors) {
			auto pred = weak_pred.lock();
			if (!pred)
				throw std::runtime_error("Couldn't lock predecessor of " + label);
			if (cfg.hasLabel(pred->label)) {
				cfg.link(pred->label, label);
			} else {
				warn() << "Predicate \e[1m" << pred->label << "\e[22m doesn't correspond to any CFG node in "
							"function \e[1m" << function.name << "\e[22m\n";
				for (const auto &pair: cfg)
					std::cerr << "- " << pair.first << '\n';
			}
		}

		if (!block->instructions.empty()) {
			auto &back = block->instructions.back();
			// if (back->isTerminal()) {
			// 	cfg.link(*label, "exit");
			// 	exit_linked = true;
			// } else
			if (auto *jump = back->cast<JumpInstruction>()) {
				if (jump->imm == label) {
					// The block unconditionally branches to itself, meaning it's an infinite loop.
					// Let's pretend for the sake of the DTree algorithms that it's connected to the exit.
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
		cfg.link(function.blocks.back()->label, "exit");

	// try {
	// 	function.dTree.emplace(cfg, cfg[0]);
	// } catch (std::exception &err) {
	// 	error() << "Constructing DTree failed in function " << *function.name << " for start node "
	// 	        << cfg[0].label() << ": " << err.what() << std::endl;
	// 	cfg.renderTo("cfg_error.png");
	// 	throw;
	// }
	// function.dTree->name = "DTree";
	// function.djGraph.emplace(cfg, cfg[0]);
	// function.djGraph->name = "DJ Graph";
	// walkCFG(function, cfg, 1000, 0, 1000);
	return cfg;
}

// void walkCFG(Function &function, Graph &cfg, size_t walks = 1, unsigned seed = 0, size_t inner_limit = 1000) {
// 	srand(seed == 0? time(NULL) : seed);

// 	for (size_t walk = 0; walk < walks; ++walk) {
// 		Node *node = &cfg[0], *end = &cfg["exit"];
// 		size_t count = 0;
// 		// End the walk once we reach the exit or until we've reached the maximum number of moves allowed per walk.
// 		while (node != end && ++count <= inner_limit) {
// 			// Increase the estimated execution count of the node we just walked to.
// 			++node->get<std::weak_ptr<BasicBlock>>().lock()->estimatedExecutions;
// 			// Check the number of outward edges.
// 			size_t out_count = node->out().size();
// 			if (out_count == 0) {
// 				// If it's somehow zero, the walk is over.
// 				break;
// 			} else if (out_count == 1) {
// 				// If it's just one, simply go to the next node.
// 				node = *node->out().begin();
// 			} else {
// 				// Otherwise, if there are multiple options, choose one randomly.
// 				node = *std::next(node->out().begin(), rand() % out_count);
// 			}
// 		}
// 	}

// 	function.walkCount += walks;
// }
