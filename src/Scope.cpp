#include <sstream>

#include "Errors.h"
#include "Function.h"
#include "Program.h"
#include "Scope.h"

FunctionPtr Scope::lookupFunction(const std::string &function_name, TypePtr return_type, const Types &arg_types,
                                  const std::string &struct_name, const ASTLocation &location) const {
	Functions results = lookupFunctions(function_name, return_type, arg_types, struct_name);
	if (1 < results.size()) {
		std::stringstream error;
		error << "Multiple results found for " << *return_type << ' ' << function_name << '(';
		for (size_t i = 0, max = arg_types.size(); i < max; ++i) {
			if (i != 0)
				error << ", ";
			error << *arg_types.at(i);
		}
		error << ')';
		throw LocatedError(location, error.str());
	}

	return results.empty()? nullptr : results.front();
}

FunctionPtr Scope::lookupFunction(const std::string &function_name, const Types &arg_types,
                                  const std::string &struct_name, const ASTLocation &location) const {
	Functions results = lookupFunctions(function_name, arg_types, struct_name);
	if (1 < results.size()) {
		std::stringstream error;
		error << "Multiple results found for " << function_name << '(';
		for (size_t i = 0, max = arg_types.size(); i < max; ++i) {
			if (i != 0)
				error << ", ";
			error << *arg_types.at(i);
		}
		error << ')';
		throw LocatedError(location, error.str());
	}

	return results.empty()? nullptr : results.front();
}

VariablePtr EmptyScope::lookup(const std::string &) const {
	return nullptr;
}

bool EmptyScope::doesConflict(const std::string &) const {
	return false;
}

bool EmptyScope::insert(VariablePtr) {
	return false;
}

VariablePtr BasicScope::lookup(const std::string &name) const {
	if (variables.count(name) == 0)
		return nullptr;
	return variables.at(name);
}

bool BasicScope::doesConflict(const std::string &name) const {
	return variables.count(name) != 0;
}

bool BasicScope::insert(VariablePtr variable) {
	if (doesConflict(variable->name))
		return false;
	variables.emplace(variable->name, variable);
	return true;
}

FunctionScope::FunctionScope(Function &function_, std::shared_ptr<GlobalScope> parent_):
	Scope(&function_.program), function(function_), parent(parent_) {}

VariablePtr FunctionScope::lookup(const std::string &name) const {
	if (function.variables.count(name) == 0)
		return parent->lookup(name);
	return function.variables.at(name);
}

Functions FunctionScope::lookupFunctions(const std::string &function_name, TypePtr return_type, const Types &arg_types,
                                         const std::string &struct_name) const {
	return parent->lookupFunctions(function_name, return_type, arg_types, struct_name);
}

Functions FunctionScope::lookupFunctions(const std::string &function_name, const Types &arg_types,
                                         const std::string &struct_name) const {
	return parent->lookupFunctions(function_name, arg_types, struct_name);
}

TypePtr FunctionScope::lookupType(const std::string &name) const {
	return parent->lookupType(name);
}

bool FunctionScope::doesConflict(const std::string &name) const {
	// It's fine for local variables to shadow global variables.
	return function.variables.count(name) != 0;
}

bool FunctionScope::insert(VariablePtr variable) {
	if (doesConflict(variable->name))
		return false;
	function.variables.emplace(variable->name, variable);
	return true;
}

VariablePtr GlobalScope::lookup(const std::string &name) const {
	if (program.globals.count(name) == 0)
		return nullptr;
	return program.globals.at(name);
}

Functions GlobalScope::lookupFunctions(const std::string &function_name, TypePtr return_type, const Types &arg_types,
                                       const std::string &struct_name) const {
	Functions out;
	std::set<std::string> found_manglings;

	{
		auto [begin, end] = program.bareFunctions.equal_range(function_name);
		for (auto iter = begin; iter != end; ++iter) {
			bool should_add = false;
			if (!return_type) {
				should_add = struct_name.empty()
					|| (iter->second->structParent && iter->second->structParent->name == struct_name);
			} else
				should_add = iter->second->isMatch(return_type, arg_types, struct_name);
			if (should_add) {
				out.push_back(iter->second);
				found_manglings.insert(iter->second->mangle());
			}
		}
	} {
		auto [begin, end] = program.bareFunctionDeclarations.equal_range(function_name);
		for (auto iter = begin; iter != end; ++iter) {
			bool should_add = false;
			if (!return_type) {
				should_add = struct_name.empty()
					|| (iter->second->structParent && iter->second->structParent->name == struct_name);
			} else
				should_add = iter->second->isMatch(return_type, arg_types, struct_name);
			if (should_add && found_manglings.count(iter->second->mangle()) == 0)
					out.push_back(iter->second);
		}
	}

	return out;
}

Functions GlobalScope::lookupFunctions(const std::string &function_name, const Types &arg_types, const std::string &struct_name) const {
	Functions out;
	std::set<std::string> found_manglings;

	{
		auto [begin, end] = program.bareFunctions.equal_range(function_name);
		for (auto iter = begin; iter != end; ++iter)
			if (iter->second->isMatch(nullptr, arg_types, struct_name)) {
				out.push_back(iter->second);
				found_manglings.insert(iter->second->mangle());
			}
	} {
		auto [begin, end] = program.bareFunctionDeclarations.equal_range(function_name);
		for (auto iter = begin; iter != end; ++iter)
			if (iter->second->isMatch(nullptr, arg_types, struct_name))
				if (found_manglings.count(iter->second->mangle()) == 0)
					out.push_back(iter->second);
	}

	return out;
}

TypePtr GlobalScope::lookupType(const std::string &name) const {
	if (program.structs.count(name) != 0)
		return program.structs.at(name);
	return nullptr;
}

bool GlobalScope::doesConflict(const std::string &name) const {
	return program.globals.count(name) != 0;
}

bool GlobalScope::insert(VariablePtr variable) {
	if (!variable->is<Global>())
		throw std::invalid_argument("Can't insert a non-global into a GlobalScope");

	if (doesConflict(variable->name))
		return false;

	program.globals.emplace(variable->name, std::dynamic_pointer_cast<Global>(variable));
	return true;
}

VariablePtr BlockScope::lookup(const std::string &name) const {
	if (variables.count(name) != 0)
		return variables.at(name);
	return parent->lookup(name);
}

Functions BlockScope::lookupFunctions(const std::string &function_name, TypePtr return_type, const Types &arg_types,
                                      const std::string &struct_name) const {
	return parent->lookupFunctions(function_name, return_type, arg_types, struct_name);
}

Functions BlockScope::lookupFunctions(const std::string &function_name, const Types &arg_types,
                                      const std::string &struct_name) const {
	return parent->lookupFunctions(function_name, arg_types, struct_name);
}

TypePtr BlockScope::lookupType(const std::string &name) const {
	return parent->lookupType(name);
}

bool BlockScope::doesConflict(const std::string &name) const {
	// It's fine if the parent scope has a conflict because it can be shadowed in this scope.
	return variables.count(name) != 0;
}

bool BlockScope::insert(VariablePtr variable) {
	if (doesConflict(variable->name))
		return false;
	variables.emplace(variable->name, variable);
	return true;
}
