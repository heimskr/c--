#include <memory>
#include <stdexcept>
#include <string>

struct Scope;

using ScopePtr = std::shared_ptr<Scope>;

struct ResolutionError: std::runtime_error {
	ScopePtr scope;
	std::string name;
	ResolutionError(const std::string &name_, ScopePtr scope_):
		std::runtime_error("Couldn't resolve symbol " + name_), scope(scope_), name(name_) {}
};
