#include "Errors.h"
#include "Function.h"
#include "Immediate.h"
#include "Util.h"
#include "Variable.h"

std::string stringify(const Immediate &imm, bool colored, bool ampersand) {
	if (std::holds_alternative<int>(imm)) {
		const std::string str = std::to_string(std::get<int>(imm));
		return colored? "\e[36m" + str + "\e[39m" : str;
	}
	if (std::holds_alternative<VariablePtr>(imm)) {
		const auto &var = std::get<VariablePtr>(imm);
		if (var->function == nullptr)
			throw std::runtime_error("Variable " + var->name + " has no function");
		if (var->function->stackOffsets.count(var) == 0)
			throw NotOnStackError(var);
		return std::to_string(var->function->stackOffsets.at(var));
	}
	const auto &str = std::get<std::string>(imm);
	return colored? "\e[38;5;202m" + (ampersand? "&" + str : str) + "\e[39m" : (ampersand? "&" + str : str);
}

std::string charify(const Immediate &imm) {
	if (!std::holds_alternative<int>(imm))
		throw std::invalid_argument("Cannot charify a non-int Immediate");
	std::string hex = Util::hex(std::get<int>(imm));
	return "'\\x" + (hex.size() == 1? "0" + hex : hex) + "'";
}

bool operator==(const Immediate &imm, const std::string &str) {
	return std::holds_alternative<std::string>(imm) && std::get<std::string>(imm) == str;
}
