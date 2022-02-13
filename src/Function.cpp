#include "ASTNode.h"
#include "Function.h"
#include "Util.h"

std::string Function::compile() {
	std::vector<std::string> instruction_strings;

	for (const auto &instruction: instructions)
		instruction_strings.push_back(std::string(*instruction));

	return Util::join(instruction_strings, "\n");
}
