#include "Errors.h"
#include "Function.h"
#include "Immediate.h"
#include "Variable.h"

std::string stringify(const Immediate &imm) {
	if (std::holds_alternative<int>(imm))
		return std::to_string(std::get<int>(imm));
	if (std::holds_alternative<VariablePtr>(imm)) {
		const auto &var = std::get<VariablePtr>(imm);
		if (!var->function)
			throw std::runtime_error("Variable " + var->name + " has no function");
		if (var->function->stackOffsets.count(var) == 0)
			throw NotOnStackError(var);
		return std::to_string(var->function->stackOffsets.at(var));
	}
	return std::get<std::string>(imm);
}