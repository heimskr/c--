#pragma once

#include <string>

#include "ASTNode.h"
#include "Hash.h"

class Function;

struct DebugData {
	std::string mangledFunction;
	ASTLocation location;
	DebugData(const ASTLocation &location_ = {}, const std::string &mangled_function = {}):
		mangledFunction(mangled_function), location(location_) {}
	DebugData(const ASTLocation &, const Function &);
	bool operator<(const DebugData &) const;
};

namespace std {
	template <>
	struct hash<DebugData> {
		size_t operator()(const DebugData &data) {
			return std::hash<std::string>()(data.mangledFunction) ^ Hash<ASTLocation>()(data.location);
		}
	};
}
