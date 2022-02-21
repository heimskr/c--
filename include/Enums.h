#pragma once

#include <string>
#include <unordered_map>

enum class Condition {Positive, Negative, Zero, Nonzero, None};
enum class Comparison {Eq, Neq, Lt, Lte, Gt, Gte};
enum class PrintType {Dec, Bin, Hex, Char, Full};
enum class QueryType {Memory};

extern std::unordered_map<Comparison, std::string> comparison_map;
extern std::unordered_map<QueryType,  std::string> query_map;

// extern std::unordered_map<TypeType,   std::string> type_map;
// extern std::unordered_map<ValueType,  std::string> value_map;
// extern std::unordered_map<Linkage,    std::string> linkage_map;
// extern std::unordered_map<Preemption, std::string> preemption_map;
// extern std::unordered_map<CConv,      std::string> cconv_map;
// extern std::unordered_map<RetAttr,    std::string> retattr_map;
// extern std::unordered_map<ParAttr,    std::string> parattr_map;
// extern std::unordered_map<FnAttr,     std::string> fnattr_map;
// extern std::unordered_map<Fastmath,   std::string> fastmath_map;
// extern std::unordered_map<Ordering,   std::string> ordering_map;
// extern std::unordered_map<IcmpCond,   std::string> cond_map;
// extern std::unordered_map<IcmpCond,   std::string> cond_op_map;
// extern std::unordered_map<std::string,   IcmpCond> cond_inv_map;
// extern std::unordered_map<IcmpCond,      IcmpCond> cond_rev_map;
// extern std::unordered_map<Conversion, std::string> conversion_map;
// extern std::unordered_map<LogicType,  std::string> logic_map;
// extern std::unordered_map<std::string,  LogicType> logic_inv_map;

