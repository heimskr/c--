#pragma once

#include <memory>

class Function;
struct Program;
struct Scope;

struct Context {
	std::shared_ptr<Scope> scope;
	std::string structName;
	Program *program = nullptr;

	Context(Program &program_, std::shared_ptr<Scope> scope_ = nullptr, const std::string &struct_name = ""):
		scope(scope_), structName(struct_name), program(&program_) {}
};