#include "ASTNode.h"
#include "Expr.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
#include "Scope.h"
#include "Type.h"
#include "Why.h"
#include "WhyInstructions.h"

Program compileRoot(const ASTNode &root) {
	Program out;

	for (const ASTNode *child: root)
		switch (child->symbol) {
			case CMMTOK_FN: {
				const std::string &name = *child->front()->lexerInfo;
				if (out.signatures.count(name) != 0)
					throw std::runtime_error("Cannot redefine function " + name);
				decltype(Signature::argumentTypes) args;
				for (const ASTNode *arg: *child->at(2))
					args.emplace_back(getType(*arg->front()));
				out.signatures.try_emplace(name, std::shared_ptr<Type>(getType(*child->at(1))), std::move(args));
				out.functions.try_emplace(name, out, child);
				break;
			}
			case CMMTOK_COLON: { // Global variable
				const std::string &name = *child->front()->lexerInfo;
				if (out.globals.count(name) != 0)
					throw std::runtime_error("Cannot redefine global " + name);
				auto type = std::shared_ptr<Type>(getType(*child->at(1)));
				if (child->size() <= 2)
					out.globalOrder.push_back(out.globals.try_emplace(name,
						std::make_shared<Global>(name, type, nullptr)).first);
				else
					out.globalOrder.push_back(out.globals.try_emplace(name, std::make_shared<Global>(name, type,
						std::shared_ptr<Expr>(Expr::get(*child->at(2), &out.init)))).first);
				break;
			}
			default:
				throw std::runtime_error("Unexpected token under root: " +
					std::string(cmmParser.getName(child->symbol)));
		}

	return out;
}

void Program::compile() {
	lines.clear();
	lines.push_back("#text");
	lines.push_back("%data");
	auto init_scope = std::make_shared<GlobalScope>(*this);
	for (const auto &iter: globalOrder) {
		const auto &expr = iter->second->value;
		lines.push_back("@" + iter->first);
		auto size = iter->second->type->getSize();
		if (expr) {
			auto value = expr->evaluate();
			if (value && size == 1) {
				lines.push_back("%1b " + std::to_string(*value));
			} else if (value && size == 2) {
				lines.push_back("%2b " + std::to_string(*value));
			} else if (value && size == 4) {
				lines.push_back("%4b " + std::to_string(*value));
			} else if (value && size == 8) {
				lines.push_back("%8b " + std::to_string(*value));
			} else {
				lines.push_back("%fill " + std::to_string(size) + " 0");
				VregPtr vreg = std::make_shared<VirtualRegister>(init);
				vreg->reg = Why::temporaryOffset;
				expr->compile(vreg, init, init_scope);
				init.why.emplace_back(new StoreIInstruction(vreg, iter->first));
			}
		} else if (size == 1) {
			lines.push_back("%1b 0");
		} else if (size == 2) {
			lines.push_back("%2b 0");
		} else if (size == 4) {
			lines.push_back("%4b 0");
		} else {
			lines.push_back("%8b 0");
		}
	}

	const auto init_lines = init.stringify();

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