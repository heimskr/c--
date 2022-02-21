#include "Enums.h"

std::unordered_map<Comparison, std::string> comparison_map {
	{Comparison::Eq,  "=="}, {Comparison::Neq, "!="}, {Comparison::Lt,  "<"},
	{Comparison::Lte, "<="}, {Comparison::Gt,  ">"},  {Comparison::Gte, ">="}};

std::unordered_map<QueryType, std::string> query_map {{QueryType::Memory, "mem"}};
