#include "Casting.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Type.h"
#include "Variable.h"
#include "WhyInstructions.h"

bool tryCast(const Type &right_type, const Type &left_type, VregPtr vreg, Function &function) {
	if (!(right_type && left_type)) {
		if (right_type.isInt() && left_type.isInt()) {
			std::cerr << "tryCast(" << right_type << ", " << left_type << ", " << *vreg << ", " << function.name << ")\n";
			const auto *right_int = right_type.cast<IntType>(), *left_int = left_type.cast<IntType>();
			if (right_type.isSigned() && left_type.isSigned()) {
				if (right_int->width < left_int->width)
					function.add<SextInstruction>(vreg, vreg, left_int->width);
				else
					function.add<AndIInstruction>(vreg, vreg, int((1ul << left_int->width) - 1));
			} else if (left_int->width < right_int->width)
				function.add<AndIInstruction>(vreg, vreg, int((1ul << left_int->width) - 1));
			return true;
		}
		return false;
	}
	return true;
}

void typeCheck(const Type &right_type, const Type &left_type, VregPtr vreg, Function &function) {
	if (!tryCast(right_type, left_type, vreg, function))
		throw ImplicitConversionError(TypePtr(right_type.copy()), TypePtr(left_type.copy()));
}
