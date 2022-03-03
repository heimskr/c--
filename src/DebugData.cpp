#include "DebugData.h"
#include "Function.h"

DebugData::DebugData(const ASTLocation &location, const Function &function):
	DebugData(location, function.mangle()) {}

bool DebugData::operator<(const DebugData &other) const {
	if (mangledFunction < other.mangledFunction)
		return true;
	if (mangledFunction > other.mangledFunction)
		return false;
	if (location.line < other.location.line)
		return true;
	if (location.line > other.location.line)
		return false;
	return location.column < other.location.column;
}

DebugData::operator bool() const {
	return !mangledFunction.empty() && location.column != 0;
}
