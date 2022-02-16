#include <memory>
#include <stdexcept>
#include <string>

class Function;
struct Scope;
struct Variable;

using ScopePtr = std::shared_ptr<Scope>;
using VariablePtr = std::shared_ptr<Variable>;

struct ResolutionError: std::runtime_error {
	ScopePtr scope;
	std::string name;
	ResolutionError(const std::string &name_, ScopePtr scope_):
		std::runtime_error("Couldn't resolve symbol " + name_), scope(scope_), name(name_) {}
};

struct LvalueError: std::runtime_error {
	std::string typeString;
	LvalueError(const std::string &type_string):
		std::runtime_error("Not an lvalue: " + type_string), typeString(type_string) {}
};

struct NotOnStackError: std::runtime_error {
	VariablePtr variable;
	NotOnStackError(VariablePtr);
};
