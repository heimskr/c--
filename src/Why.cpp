#include <tuple>

#include "ASTNode.h"
#include "Util.h"
#include "Why.h"

std::set<int> Why::makeRegisterPool() {
	std::set<int> out;
	for (int i = temporaryOffset, max = temporaryOffset + temporaryCount; i < max; ++i)
		out.insert(i);
	for (int i = savedOffset, max = savedOffset + savedCount; i < max; ++i)
		out.insert(i);
	return out;
}

bool Why::isSpecialPurpose(int reg) {
	return 0 <= reg && reg < 128 && (reg < temporaryOffset || savedOffset + savedCount <= reg);
}

bool Why::isGeneralPurpose(int reg) {
	return temporaryOffset <= reg && reg < savedOffset + savedCount;
}

bool Why::isArgumentRegister(int reg) {
	return argumentOffset <= reg && reg < argumentOffset + argumentCount;
}

std::string Why::registerName(int reg) {
	switch (reg) {
		case              zeroOffset: return "0";
		case globalAreaPointerOffset: return "g";
		case      stackPointerOffset: return "sp";
		case      framePointerOffset: return "fp";
		case     returnAddressOffset: return "rt";
		case                loOffset: return "lo";
		case                hiOffset: return "hi";
		case            statusOffset: return "st";
		default: break;
	}

	static std::initializer_list<std::tuple<const int, const int, char>> list {
		{returnValueOffset, returnValueCount, 'r'},
		{argumentOffset,    argumentCount,    'a'},
		{temporaryOffset,   temporaryCount,   't'},
		{savedOffset,       savedCount,       's'},
		{kernelOffset,      kernelCount,      'k'},
		{assemblerOffset,   assemblerCount,   'm'},
		{floatingOffset,    floatingCount,    'f'},
		{exceptionOffset,   exceptionCount,   'e'},
	};

	for (auto [offset, count, prefix]: list) {
		if (offset <= reg && reg < offset + count)
			return prefix + Util::hex(reg - offset);
	}

	return "[" + std::to_string(reg) + "?]";
}

std::string Why::coloredRegister(int reg) {
	switch (reg) {
		case              zeroOffset: return "\e[90m$0\e[39m";
		case globalAreaPointerOffset: return "\e[36m$g\e[39m";
		case      stackPointerOffset: return "\e[31m$sp\e[39m";
		case      framePointerOffset: return "\e[35m$fp\e[39m";
		case     returnAddressOffset: return "\e[32m$rt\e[39m";
		case                loOffset: return "\e[31m$lo\e[39m";
		case                hiOffset: return "\e[31m$hi\e[39m";
		case            statusOffset: return "$st";
		default: break;
	}

	static std::initializer_list<std::tuple<const int, const int, const char *>> list {
		{returnValueOffset, returnValueCount, "\e[31m$r"},
		{argumentOffset,    argumentCount,    "\e[38;5;202m$a"},
		{temporaryOffset,   temporaryCount,   "\e[33m$t"},
		{savedOffset,       savedCount,       "\e[32m$s"},
		{kernelOffset,      kernelCount,      "\e[38;5;33m$k"},
		{exceptionOffset,   exceptionCount,   "\e[38;5;88m$e"},
		{assemblerOffset,   assemblerCount,   "\e[38;5;129m$m"},
		{floatingOffset,    floatingCount,    "\e[38;5;126m$f"},
	};

	for (auto [offset, count, prefix]: list) {
		if (offset <= reg && reg < offset + count) {
			std::stringstream out;
			out << prefix << std::hex << (reg - offset) << "\e[39m";
			return out.str();
		}
	}

	return "$[" + std::to_string(reg) + "?]";
}

OperandType::OperandType(const ASTNode *node) {
	if (node == nullptr)
		return;

	auto inner = std::string_view(*node->text);
	inner.remove_prefix(1); // Remove initial '{'
	if (inner.front() == 'v') {
		primitive = Primitive::Void;
		inner.remove_prefix(1);
		isSigned = false;
	} else {
		isSigned = inner.front() == 's';
		inner.remove_prefix(1);
		switch (inner.front()) {
			case 'c': primitive = Primitive::Char;  break;
			case 's': primitive = Primitive::Short; break;
			case 'i': primitive = Primitive::Int;   break;
			case 'l': primitive = Primitive::Long;  break;
		}
		inner.remove_prefix(1);
	}

	pointerLevel = static_cast<int>(inner.size()) - 1; // Inner matches /^\**\}$/ at this point.
}

OperandType OperandType::VOID_PTR = OperandType(false, Primitive::Void, 1);
OperandType OperandType::ULONG    = OperandType(false, Primitive::Long, 0);
OperandType OperandType::VOID     = OperandType(false, Primitive::Void, 0);
OperandType OperandType::CHAR_PTR = OperandType(false, Primitive::Char, 1);

TypedReg::TypedReg(const ASTNode *node):
	type(node), reg(node? node->front()->text : nullptr) {}
