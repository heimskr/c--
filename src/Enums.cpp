#include "Enums.h"
#include "Lexer.h"

std::unordered_map<Comparison, std::string> comparison_map {
	{Comparison::Eq,  "=="}, {Comparison::Neq, "!="}, {Comparison::Lt,  "<"},
	{Comparison::Lte, "<="}, {Comparison::Gt,  ">"},  {Comparison::Gte, ">="}};

std::unordered_map<QueryType, std::string> query_map {{QueryType::Memory, "mem"}};

std::unordered_map<int, std::string> operator_map {
	{CMMTOK_PLUS,  "pl"}, {CMMTOK_MINUS,   "mi"}, {CMMTOK_TIMES, "ti"}, {CMMTOK_DIV,    "di"}, {CMMTOK_MOD,     "mo"},
	{CMMTOK_AND,   "an"}, {CMMTOK_OR,      "or"}, {CMMTOK_XOR,   "xo"}, {CMMTOK_NOT,    "nt"}, {CMMTOK_TILDE,   "td"},
	{CMMTOK_LAND,  "la"}, {CMMTOK_LOR,     "lo"}, {CMMTOK_LXOR,  "lx"}, {CMMTOK_LSHIFT, "ls"}, {CMMTOK_RSHIFT,  "rs"},
	{CMMTOK_LT,    "lt"}, {CMMTOK_GT,      "gt"}, {CMMTOK_LTE,   "le"}, {CMMTOK_GTE,    "ge"}, {CMMTOK_DEQ,     "eq"},
	{CMMTOK_NEQ,   "ne"}, {CMMTOK_ASSIGN,  "as"}, {CMMTOK_ARROW, "ar"}, {CMMTOK_PLUSEQ, "Pl"}, {CMMTOK_MINUSEQ, "Mi"},
	{CMMTOK_SREQ,  "Rs"}, {CMMTOK_MODEQ,   "Mo"}, {CMMTOK_SLEQ,  "Ls"}, {CMMTOK_DIVEQ,  "Di"}, {CMMTOK_TIMESEQ, "Ti"},
	{CMMTOK_ANDEQ, "An"}, {CMMTOK_OREQ,    "Or"}, {CMMTOK_XOREQ, "Xo"}, {CMM_ACCESS,    "ac"}, {CMMTOK_LPAREN,  "ca"},
	{CMMTOK_PLUSPLUS, "pp"}, {CMMTOK_MINUSMINUS, "mm"}, {CMM_POSTPLUS, "Pp"}, {CMM_POSTMINUS, "Mm"}, {CMMTOK_HASH, "ha"}
};

std::unordered_map<std::string, int> operator_str_map {
	{"+",   CMMTOK_PLUS},  {"-",  CMMTOK_MINUS},  {"*",   CMMTOK_TIMES}, {"/",  CMMTOK_DIV},    {"%",  CMMTOK_MOD},
	{"&",   CMMTOK_AND},   {"|",  CMMTOK_OR},     {"^",   CMMTOK_XOR},   {"!",  CMMTOK_NOT},    {"~",  CMMTOK_TILDE},
	{"&&",  CMMTOK_LAND},  {"||", CMMTOK_LOR},    {"^^",  CMMTOK_LXOR},  {"<<", CMMTOK_LSHIFT}, {">>", CMMTOK_RSHIFT},
	{"<",   CMMTOK_LT},    {">",  CMMTOK_GT},     {"<=",  CMMTOK_LTE},   {">=", CMMTOK_GTE},    {"==", CMMTOK_DEQ},
	{"!=",  CMMTOK_NEQ},   {"=",  CMMTOK_ASSIGN}, {"->",  CMMTOK_ARROW}, {"+=", CMMTOK_PLUSEQ}, {"-=", CMMTOK_MINUSEQ},
	{">>=", CMMTOK_SREQ},  {"%=", CMMTOK_MODEQ},  {"<<=", CMMTOK_SLEQ},  {"/=", CMMTOK_DIVEQ},  {"*=", CMMTOK_TIMESEQ},
	{"&=",  CMMTOK_ANDEQ}, {"|=", CMMTOK_OREQ},   {"^=",  CMMTOK_XOREQ}, {"[",  CMM_ACCESS},    {"(",  CMMTOK_LPAREN},
	{"++.", CMMTOK_PLUSPLUS}, {"--.", CMMTOK_MINUSMINUS}, {".++", CMM_POSTPLUS}, {".--", CMM_POSTMINUS},
	{"#",   CMMTOK_HASH}
};
