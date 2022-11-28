#include "Errors.h"
#include "Function.h"
#include "Global.h"
#include "Immediate.h"
#include "Util.h"
#include "Variable.h"

TypedImmediate::TypedImmediate(const Global &global):
	TypedImmediate(static_cast<OperandType>(*global.getType()), global.name) {}

std::string stringify(const TypedImmediate &imm, bool colored, bool ampersand) {
	if (imm.is<int>()) {
		const std::string str = std::to_string(imm.get<int>());
		std::string out = colored? "\e[36m" + str + "\e[39m" : str;
		return out + std::string(imm.type);
	}
	if (imm.is<VariablePtr>()) {
		const auto &var = imm.get<VariablePtr>();
		if (var->function == nullptr)
			throw std::runtime_error("Variable " + var->name + " has no function");
		if (var->function->stackOffsets.count(var) == 0)
			throw NotOnStackError(var);
		return std::to_string(var->function->stackOffsets.at(var)) + std::string(imm.type);
	}
	const auto &str = imm.get<std::string>();
	std::string out = colored? "\e[38;5;202m" + (ampersand? "&" + str : str) + "\e[39m" : (ampersand? "&" + str : str);
	return out + std::string(imm.type);
}

std::string charify(const TypedImmediate &imm) {
	if (!imm.is<int>())
		throw std::invalid_argument("Cannot charify a non-int TypedImmediate");
	const std::string hex = Util::hex(imm.get<int>());
	return "'\\x" + (hex.size() == 1? "0" + hex : hex) + '\'';
}

bool operator==(const TypedImmediate &imm, const std::string &str) {
	return imm.is<std::string>() && imm.get<std::string>() == str;
}
