#pragma once

#include <string>
#include <unordered_map>

enum class Condition {Positive, Negative, Zero, Nonzero, None};
enum class Comparison {Eq, Neq, Lt, Lte, Gt, Gte};
enum class PrintType {Dec, Bin, Hex, Char, Full};
enum class QueryType {Memory};

extern std::unordered_map<Comparison, std::string> comparison_map;
extern std::unordered_map<QueryType,  std::string> query_map;
extern std::unordered_map<int, std::string> operator_map;
extern std::unordered_map<std::string, std::string> operator_str_map;
