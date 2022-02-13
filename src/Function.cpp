#include "ASTNode.h"
#include "CmmInstructions.h"
#include "Function.h"
#include "Util.h"
#include "WhyInstructions.h"

std::vector<std::string> Function::stringify() {
	std::vector<std::string> out;
	for (const auto &instruction: why)
		for (const std::string &line: std::vector<std::string>(*instruction))
			out.push_back(line);
	return out;
}

void Function::compile() {

}
