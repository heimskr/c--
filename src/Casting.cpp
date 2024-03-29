#include <cassert>

#include "Casting.h"
#include "Errors.h"
#include "Expr.h"
#include "Function.h"
#include "Type.h"
#include "Variable.h"
#include "WhyInstructions.h"

bool tryCast(const Type &right_type, const Type &left_type, const VregPtr &vreg, Function &function,
             const ASTLocation &location) {
	const Type *left_subtype = &left_type;
	if (left_type.isReference())
		left_subtype = left_type.cast<ReferenceType>()->subtype;

	const Type *right_subtype = &right_type;
	if (right_type.isReference())
		right_subtype = right_type.cast<ReferenceType>()->subtype;

	if (!(*right_subtype && *left_subtype)) {
		if (right_subtype->isInt() && left_subtype->isInt()) {
			const auto *right_int = right_subtype->cast<IntType>();
			const auto *left_int  = left_subtype->cast<IntType>();
			if (vreg) {
				if (right_subtype->isSigned(0) && left_subtype->isSigned(0)) {
					if (right_int->width < left_int->width)
						function.add<SextInstruction>(vreg, vreg, right_int->width)->setDebug({location, function});
					if (left_int->width != 64) {
						assert(((1ul << left_int->width) - 1) <= UINT_MAX);
						function.add<AndIInstruction>(vreg, vreg, TypedImmediate(OperandType(*vreg->getType()),
							static_cast<int>((1ul << left_int->width) - 1)))->setDebug({location, function});
					}
				} else if (left_int->width < right_int->width) {
					assert(((1ul << left_int->width) - 1) <= UINT_MAX);
					function.add<AndIInstruction>(vreg, vreg, 
						TypedImmediate(OperandType(*vreg->getType()), static_cast<int>((1ul << left_int->width) - 1)))
						->setDebug({location, function});
				}
			}
			return true;
		}
		return false;
	}
	return true;
}

void typeCheck(const Type &right_type, const Type &left_type, const VregPtr &vreg, Function &function,
               const ASTLocation &location) {
	if (!tryCast(right_type, left_type, vreg, function, location))
		throw ImplicitConversionError(TypePtr(right_type.copy()), TypePtr(left_type.copy()), location);
}
