#include "Enums.h"

std::unordered_map<Comparison, std::string> comparison_map {
	{Comparison::Eq,  "Eq"},  {Comparison::Neq, "Neq"}, {Comparison::Lt,  "Lt"},
	{Comparison::Lte, "Lte"}, {Comparison::Gt,  "Gt"},  {Comparison::Gte, "Gte"}};
