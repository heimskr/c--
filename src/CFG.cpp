#include "CFG.h"
#include "Function.h"

Graph makeCFG(Function &function) {
	Graph cfg;
	
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
