#include "Errors.h"
#include "Function.h"
#include "Immediate.h"
#include "Variable.h"

std::string stringify(const Immediate &imm, bool colored) {
	if (std::holds_alternative<int>(imm)) {
		const std::string str = std::to_string(std::get<int>(imm));
		return colored? "\e[36m" + str + "\e[39m" : str;
	}
	if (std::holds_alternative<VariablePtr>(imm)) {
		const auto &var = std::get<VariablePtr>(imm);
		if (!var->function)
			throw std::runtime_error("Variable " + var->name + " has no function");
		if (var->function->stackOffsets.count(var) == 0)
			throw NotOnStackError(var);
		return std::to_string(var->function->stackOffsets.at(var));
	}
	const std::string &str = std::get<std::string>(imm);
	return colored? "\e[38;5;202m" + str + "\e[39m" : str;
}

bool operator==(const Immediate &imm, const std::string &str) {
	return std::holds_alternative<std::string>(imm) && std::get<std::string>(imm) == str;
}
