#include "ASTNode.h"
#include "Expr.h"
#include "Lexer.h"

std::string stringify(const Expr *expr) {
	if (!expr)
		return "...";
	if (dynamic_cast<const BinaryExpr *>(expr))
		return "(" + std::string(*expr) + ")";
	return std::string(*expr);
}

Expr * Expr::get(const ASTNode &node) {
	// node.debug();
	switch (node.symbol) {
		case CMMTOK_PLUS:
			return new PlusExpr(std::unique_ptr<Expr>(Expr::get(*node.at(0))), std::unique_ptr<Expr>(Expr::get(*node.at(1))));
		case CMMTOK_NUMBER:
			return new NumberExpr(ssize_t(node.atoi()));
		default:
			return nullptr;
	}
}
