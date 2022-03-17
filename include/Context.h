#pragma once

#include <memory>

class Function;
struct Program;
struct Scope;

struct Context {
	std::shared_ptr<Scope> scope;
	std::string structName;
	Program *program = nullptr;

	explicit Context(Program &program_, std::shared_ptr<Scope> scope_ = nullptr, std::string struct_name = ""):
		scope(std::move(scope_)), structName(std::move(struct_name)), program(&program_) {}
} __attribute__((aligned(64)));