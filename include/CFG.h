#pragma once

#include "Graph.h"

class Function;

Graph makeCFG(Function &);
void walkCFG(Function &, Graph &, size_t walks = 1, unsigned seed = 0, size_t inner_limit = 1000);
