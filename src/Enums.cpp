#include "Enums.h"
#include "Lexer.h"

std::unordered_map<Comparison, std::string> comparison_map {
	{Comparison::Eq,  "=="}, {Comparison::Neq, "!="}, {Comparison::Lt,  "<"},
	{Comparison::Lte, "<="}, {Comparison::Gt,  ">"},  {Comparison::Gte, ">="}};

std::unordered_map<QueryType, std::string> query_map {{QueryType::Memory, "mem"}};

std::unordered_map<int, std::string> operator_map {
	{CPMTOK_PLUS,  "pl"}, {CPMTOK_MINUS,   "mi"}, {CPMTOK_TIMES, "ti"}, {CPMTOK_DIV,    "di"}, {CPMTOK_MOD,     "mo"},
	{CPMTOK_AND,   "an"}, {CPMTOK_OR,      "or"}, {CPMTOK_XOR,   "xo"}, {CPMTOK_NOT,    "nt"}, {CPMTOK_TILDE,   "td"},
	{CPMTOK_LAND,  "la"}, {CPMTOK_LOR,     "lo"}, {CPMTOK_LXOR,  "lx"}, {CPMTOK_LSHIFT, "ls"}, {CPMTOK_RSHIFT,  "rs"},
	{CPMTOK_LT,    "lt"}, {CPMTOK_GT,      "gt"}, {CPMTOK_LTE,   "le"}, {CPMTOK_GTE,    "ge"}, {CPMTOK_DEQ,     "eq"},
	{CPMTOK_NEQ,   "ne"}, {CPMTOK_ASSIGN,  "as"}, {CPMTOK_ARROW, "ar"}, {CPMTOK_PLUSEQ, "Pl"}, {CPMTOK_MINUSEQ, "Mi"},
	{CPMTOK_SREQ,  "Rs"}, {CPMTOK_MODEQ,   "Mo"}, {CPMTOK_SLEQ,  "Ls"}, {CPMTOK_DIVEQ,  "Di"}, {CPMTOK_TIMESEQ, "Ti"},
	{CPMTOK_ANDEQ, "An"}, {CPMTOK_OREQ,    "Or"}, {CPMTOK_XOREQ, "Xo"}, {CPM_ACCESS,    "ac"}, {CPMTOK_LPAREN,  "ca"},
	{CPMTOK_PLUSPLUS, "pp"}, {CPMTOK_MINUSMINUS, "mm"}, {CPM_POSTPLUS, "Pp"}, {CPM_POSTMINUS, "Mm"}, {CPMTOK_HASH, "ha"}
};

std::unordered_map<std::string, int> operator_str_map {
	{"+",   CPMTOK_PLUS},  {"-",  CPMTOK_MINUS},  {"*",   CPMTOK_TIMES}, {"/",  CPMTOK_DIV},    {"%",  CPMTOK_MOD},
	{"&",   CPMTOK_AND},   {"|",  CPMTOK_OR},     {"^",   CPMTOK_XOR},   {"!",  CPMTOK_NOT},    {"~",  CPMTOK_TILDE},
	{"&&",  CPMTOK_LAND},  {"||", CPMTOK_LOR},    {"^^",  CPMTOK_LXOR},  {"<<", CPMTOK_LSHIFT}, {">>", CPMTOK_RSHIFT},
	{"<",   CPMTOK_LT},    {">",  CPMTOK_GT},     {"<=",  CPMTOK_LTE},   {">=", CPMTOK_GTE},    {"==", CPMTOK_DEQ},
	{"!=",  CPMTOK_NEQ},   {"=",  CPMTOK_ASSIGN}, {"->",  CPMTOK_ARROW}, {"+=", CPMTOK_PLUSEQ}, {"-=", CPMTOK_MINUSEQ},
	{">>=", CPMTOK_SREQ},  {"%=", CPMTOK_MODEQ},  {"<<=", CPMTOK_SLEQ},  {"/=", CPMTOK_DIVEQ},  {"*=", CPMTOK_TIMESEQ},
	{"&=",  CPMTOK_ANDEQ}, {"|=", CPMTOK_OREQ},   {"^=",  CPMTOK_XOREQ}, {"[",  CPM_ACCESS},    {"(",  CPMTOK_LPAREN},
	{"++.", CPMTOK_PLUSPLUS}, {"--.", CPMTOK_MINUSMINUS}, {".++", CPM_POSTPLUS}, {".--", CPM_POSTMINUS},
	{"#",   CPMTOK_HASH}
};
