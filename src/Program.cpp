#include "ASTNode.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
#include "Type.h"

Program compileRoot(const ASTNode &root) {
	std::map<std::string, Global> globals;
	std::vector<std::map<std::string, Global>::iterator> global_order;
	std::map<std::string, Signature> signatures;
	std::map<std::string, Function> functions;

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
				functions.try_emplace(name, *child);
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
						std::shared_ptr<Type>(getType(*child->at(1))), child->at(2)).first);
				break;
			}
			default:
				throw std::runtime_error("Unexpected token under root: " +
					std::string(cmmParser.getName(child->symbol)));
		}

	return {globals, global_order, signatures, functions};
}
