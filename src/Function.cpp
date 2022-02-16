#include "ASTNode.h"
#include "CmmInstructions.h"
#include "Function.h"
#include "Lexer.h"
#include "Util.h"
#include "WhyInstructions.h"

Function::Function(Program &program_, const ASTNode *source_): program(program_), source(source_) {
	if (source) {
		name = *source->at(0)->lexerInfo;
		for (const ASTNode *child: *source->at(3)) {
			if (child->symbol == CMMTOK_COLON) {
				const std::string &var_name = *child->at(0)->lexerInfo;
				if (variables.count(var_name) != 0)
					throw std::runtime_error("Cannot redefine variable " + var_name + " in function " + name);
				variables.try_emplace(var_name, var_name, std::shared_ptr<Type>(getType(*child->at(1))), *this);
			}
		}
	}
}

std::vector<std::string> Function::stringify() {
	// std::vector<std::string> out {std::to_string(cmm.size()) + ", " + std::to_string(why.size())};
	std::vector<std::string> out;
	for (const auto &instruction: why)
		for (const std::string &line: std::vector<std::string>(*instruction))
			out.push_back(line);
	return out;
}

void Function::compile() {
	
}
