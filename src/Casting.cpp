#include "Casting.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Type.h"
#include "Variable.h"
#include "WhyInstructions.h"

bool tryCast(const Type &right_type, const Type &left_type, VregPtr vreg, Function &function,
             const ASTLocation &location) {
	if (!(right_type && left_type)) {
		if (right_type.isInt() && left_type.isInt()) {
			const auto *right_int = right_type.cast<IntType>(), *left_int = left_type.cast<IntType>();
			if (vreg) {
				if (right_type.isSigned() && left_type.isSigned()) {
					if (right_int->width < left_int->width)
						function.add<SextInstruction>(vreg, vreg, right_int->width)->setDebug({location, function});
					if (left_int->width != 64)
						function.add<AndIInstruction>(vreg, vreg, (1ul << left_int->width) - 1)->setDebug({location,
							function});
				} else if (left_int->width < right_int->width)
					function.add<AndIInstruction>(vreg, vreg, (1ul << left_int->width) - 1)->setDebug({location,
						function});
			}
			return true;
		}
		return false;
	}
	return true;
}

void typeCheck(const Type &right_type, const Type &left_type, VregPtr vreg, Function &function,
               const ASTLocation &location) {
	if (!tryCast(right_type, left_type, vreg, function, location))
		throw ImplicitConversionError(TypePtr(right_type.copy()), TypePtr(left_type.copy()), location);
}
