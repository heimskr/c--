#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "Function.h"
#include "Global.h"
#include "Signature.h"

class ASTNode;
class StructType;

struct Program {
	std::map<std::string, GlobalPtr> globals;
	std::vector<decltype(globals)::iterator> globalOrder;
	std::map<std::string, Signature> signatures;
	/** Maps mangled names to functions. */
	std::map<std::string, FunctionPtr> functions;
	/** Maps mangled names to function declarations. */
	std::map<std::string, FunctionPtr> functionDeclarations;
	/** Maps unmangled names to functions. */
	std::multimap<std::string, FunctionPtr> bareFunctions;
	/** Maps unmangled names to function declarations. */
	std::multimap<std::string, FunctionPtr> bareFunctionDeclarations;
	std::map<std::string, size_t> stringIDs;
	std::vector<std::string> lines;
	std::string name, author, orcid, version;
	std::set<std::string> forwardDeclarations;
	std::map<std::string, std::shared_ptr<StructType>> structs;
	std::string filename;

	Program() = delete;

	Program(const std::string &filename_): filename(filename_) {}

	Program(decltype(globals) &&globals_, decltype(globalOrder) &&global_order, decltype(signatures) &&signatures_,
	decltype(functions) &&functions_, const std::string &filename_):
		globals(std::move(globals_)), globalOrder(std::move(global_order)), signatures(std::move(signatures_)),
		functions(std::move(functions_)), filename(filename_) {}

	void compile();
	size_t getStringID(const std::string &);
};

Program compileRoot(const ASTNode &, const std::string &filename);
