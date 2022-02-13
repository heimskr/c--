#include "ASTNode.h"
#include "Expr.h"
#include "Function.h"
#include "Lexer.h"
#include "WhyInstructions.h"

std::string stringify(const Expr *expr) {
	if (!expr)
		return "...";
	if (expr->shouldParenthesize())
		return "(" + std::string(*expr) + ")";
	return std::string(*expr);
}

Expr * Expr::get(const ASTNode &node) {
	switch (node.symbol) {
		case CMMTOK_PLUS:
			return new PlusExpr(std::unique_ptr<Expr>(Expr::get(*node.at(0))), std::unique_ptr<Expr>(Expr::get(*node.at(1))));
		case CMMTOK_MINUS:
			return new MinusExpr(std::unique_ptr<Expr>(Expr::get(*node.at(0))), std::unique_ptr<Expr>(Expr::get(*node.at(1))));
		case CMMTOK_TIMES:
			return new MultExpr(std::unique_ptr<Expr>(Expr::get(*node.at(0))), std::unique_ptr<Expr>(Expr::get(*node.at(1))));
		case CMMTOK_NUMBER:
			return new NumberExpr(ssize_t(node.atoi()));
		case CMMTOK_TRUE:
			return new BoolExpr(true);
		case CMMTOK_FALSE:
			return new BoolExpr(false);
		default:
			return nullptr;
	}
}

void PlusExpr::compile(VregPtr destination, Function &function) const {
	VregPtr left_var = function.newVar(), right_var = function.newVar();
	left->compile(left_var, function);
	right->compile(right_var, function);
	function.why.emplace_back(new AddRInstruction(left_var, right_var, destination));
}
