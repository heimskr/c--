#pragma once

#include <memory>
#include <string>
#include <variant>

#include "Why.h"

class Function;
struct Variable;
struct Global;

using Immediate = std::variant<int, std::shared_ptr<Variable>, std::string>;

struct TypedImmediate {
	OperandType type;
	Immediate value;

	TypedImmediate(OperandType type_, Immediate value_):
		type(std::move(type_)), value(std::move(value_)) {}

	explicit TypedImmediate(const Global &);

	template <typename T>
	TypedImmediate(OperandType type_, T value_):
		type(std::move(type_)), value(value_) {}

	template <typename T>
	const T & get() const { return std::get<T>(value); }

	template <typename T>
	T & get() { return std::get<T>(value); }

	template <typename T>
	bool is() const {
		return std::holds_alternative<T>(value);
	}
};

std::string stringify(const TypedImmediate &, bool colored = false, bool ampersand = false);
std::string charify(const TypedImmediate &);
bool operator==(const TypedImmediate &, const std::string &);
