#include "ASTNode.h"
#include "Expr.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
#include "Type.h"
#include "Why.h"

Program compileRoot(const ASTNode &root) {
	std::map<std::string, Global> globals;
	std::vector<std::map<std::string, Global>::iterator> global_order;
	std::map<std::string, Signature> signatures {{"$init", {std::make_shared<VoidType>(), {}}}};
	std::map<std::string, Function> functions {{"$init", nullptr}};

	for (const ASTNode *child: root)
		switch (child->symbol) {
			case CMMTOK_FN: {
				const std::string &name = *child->front()->lexerInfo;
				if (signatures.count(name) != 0)
					throw std::runtime_error("Cannot redefine function " + name);
				decltype(Signature::argumentTypes) args;
				for (const ASTNode *arg: *child->at(2))
					args.emplace_back(getType(*arg->front()));
				signatures.try_emplace(name, std::shared_ptr<Type>(getType(*child->at(1))), std::move(args));
				functions.try_emplace(name, child);
				break;
			}
			case CMMTOK_COLON: { // Global variable
				const std::string &name = *child->front()->lexerInfo;
				if (globals.count(name) != 0)
					throw std::runtime_error("Cannot redefine global " + name);
				if (child->size() <= 2)
					global_order.push_back(globals.try_emplace(name, name,
						std::shared_ptr<Type>(getType(*child->at(1))), nullptr).first);
				else
					global_order.push_back(globals.try_emplace(name, name,
						std::shared_ptr<Type>(getType(*child->at(1))),
						std::shared_ptr<Expr>(Expr::get(*child->at(2)))).first);
				break;
			}
			default:
				throw std::runtime_error("Unexpected token under root: " +
					std::string(cmmParser.getName(child->symbol)));
		}

	return {std::move(globals), std::move(global_order), std::move(signatures), std::move(functions)};
}

void Program::compile() {
	lines.clear();
	lines.push_back("#text");
	lines.push_back("%data");
	for (const auto &iter: globalOrder) {
		const auto &expr = iter->second.value;
		lines.push_back("@" + iter->first);
		if (expr) {
			lines.push_back("%fill " + std::to_string(expr->getSize()) + " 0");
			VregPtr vreg = std::make_shared<VirtualRegister>(init);
			vreg->reg = Why::temporaryOffset;
			expr->compile(vreg, init);
		} else {
			lines.push_back("%8b 0");
		}
	}

	const auto init_lines = init.compile();

	lines.push_back("%code");
	if (!init_lines.empty())
		lines.push_back(":: $init");
	lines.push_back(":: main");
	lines.push_back("<halt>");

	if (!init_lines.empty()) {
		lines.push_back("@init");
		for (const std::string &line: init_lines)
			lines.push_back("\t" + line);
		lines.push_back("\t: $rt");
	}
}