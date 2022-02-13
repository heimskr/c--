#pragma once

#include <map>
#include <string>
#include <vector>

#include "Global.h"
#include "Signature.h"

struct Program {
	std::map<std::string, Global> globals;
	std::vector<std::map<std::string, Global>::iterator> globalOrder;
	std::map<std::string, Signature> signatures;

	Program(const decltype(globals) &globals_, const decltype(globalOrder) &global_order,
	const decltype(signatures) &signatures_):
		globals(globals_), globalOrder(global_order), signatures(signatures_) {}
};
