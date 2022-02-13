#pragma once

#include <memory>
#include <vector>

struct Type;

struct Signature {
	std::shared_ptr<Type> returnType;
	std::vector<std::shared_ptr<Type>> argumentTypes;

	Signature(std::shared_ptr<Type> return_type, const std::vector<std::shared_ptr<Type>> &argument_types):
		returnType(return_type), argumentTypes(argument_types) {}

	Signature(std::shared_ptr<Type> return_type, std::vector<std::shared_ptr<Type>> &&argument_types):
		returnType(return_type), argumentTypes(std::move(argument_types)) {}
};
