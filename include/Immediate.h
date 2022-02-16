#pragma once

#include <memory>
#include <string>
#include <variant>

class Function;
struct Variable;

using Immediate = std::variant<int, std::shared_ptr<Variable>, std::string>;

std::string stringify(const Immediate &);