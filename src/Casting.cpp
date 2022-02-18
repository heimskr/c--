#include "Casting.h"
#include "Expr.h"
#include "Function.h"
#include "Type.h"
#include "Variable.h"
#include "WhyInstructions.h"

bool tryCast(TypePtr expr_type, TypePtr var_type, VregPtr vreg, Function &function) {
	if (!(*expr_type && *var_type)) {
		if (expr_type->isInt() && var_type->isInt()) {
			auto expr_int = expr_type->ptrcast<IntType>(), var_int = var_type->ptrcast<IntType>();
			if (expr_type->isSigned() && var_type->isSigned()) {
				if (expr_int->width < var_int->width)
					function.add<SextInstruction>(vreg, vreg, var_int->width);
				else
					function.add<AndIInstruction>(vreg, vreg, int((1ul << var_int->width) - 1));
			} else if (var_int->width < expr_int->width)
				function.add<AndIInstruction>(vreg, vreg, int((1ul << var_int->width) - 1));
			return true;
		}
		return false;
	}
	return true;
}
