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

	Function &init = out.functions.try_emplace(".init", out, nullptr).first->second;
	init.name = ".init";

	for (const ASTNode *child: root)
		switch (child->symbol) {
			case CMMTOK_FN: {
				const std::string &name = *child->front()->lexerInfo;
				if (out.signatures.count(name) != 0)
					throw std::runtime_error("Cannot redefine function " + name);
				decltype(Signature::argumentTypes) args;
				for (const ASTNode *arg: *child->at(2))
					args.emplace_back(Type::get(*arg->front()));
				out.signatures.try_emplace(name, std::shared_ptr<Type>(Type::get(*child->at(1))), std::move(args));
				out.functions.try_emplace(name, out, child);
				break;
			}
			case CMMTOK_COLON: { // Global variable
				const std::string &name = *child->front()->lexerInfo;
				if (out.globals.count(name) != 0)
					throw std::runtime_error("Cannot redefine global " + name);
				auto type = std::shared_ptr<Type>(Type::get(*child->at(1)));
				if (child->size() <= 2)
					out.globalOrder.push_back(out.globals.try_emplace(name,
						std::make_shared<Global>(name, type, nullptr)).first);
				else
					out.globalOrder.push_back(out.globals.try_emplace(name, std::make_shared<Global>(name, type,
						std::shared_ptr<Expr>(Expr::get(*child->at(2), &init)))).first);
				break;
			}
			case CMMTOK_META_NAME:
				out.name = *child->front()->lexerInfo;
				break;
			case CMMTOK_META_AUTHOR:
				out.author = *child->front()->lexerInfo;
				break;
			case CMMTOK_META_ORCID:
				out.orcid = *child->front()->lexerInfo;
				break;
			case CMMTOK_META_VERSION:
				out.version = *child->front()->lexerInfo;
				break;
			default:
				throw std::runtime_error("Unexpected token under root: " +
					std::string(cmmParser.getName(child->symbol)));
		}

	// TODO: implement functions
	auto add_dummy = [&](const std::string &function_name) {
		out.functions.try_emplace("." + function_name, out, nullptr).first->second.name = "." + function_name;
	};
	add_dummy("s");
	add_dummy("c");
	add_dummy("ptr");
	for (size_t i = 8; i <= 64; i *= 2) {
		add_dummy("s" + std::to_string(i));
		add_dummy("u" + std::to_string(i));
	}

	return out;
}

void Program::compile() {
	lines = {"#meta"};
	if (!name.empty())
		lines.push_back("name: " + name);
	if (!author.empty())
		lines.push_back("author: " + author);
	if (!orcid.empty())
		lines.push_back("orcid: " + orcid);
	if (!version.empty())
		lines.push_back("version: " + version);
	lines.push_back("");
	lines.push_back("#text");
	lines.push_back("");
	lines.push_back("%data");
	auto init_scope = std::make_shared<GlobalScope>(*this);

	auto &init = functions.at(".init");

	for (const auto &iter: globalOrder) {
		const auto &expr = iter->second->value;
		lines.push_back("");
		lines.push_back("@" + iter->first);
		auto type = iter->second->type;
		auto size = type->getSize();
		if (expr) {
			auto value = expr->evaluate();
			if (value && size == 1) {
				lines.push_back("\t%1b " + std::to_string(*value));
			} else if (value && size == 2) {
				lines.push_back("\t%2b " + std::to_string(*value));
			} else if (value && size == 4) {
				lines.push_back("\t%4b " + std::to_string(*value));
			} else if (value && size == 8) {
				lines.push_back("\t%8b " + std::to_string(*value));
			} else {
				lines.push_back("\t%fill " + std::to_string(size) + " 0");

				auto expr_type = expr->getType(init.selfScope);
				VregPtr vreg = std::make_shared<VirtualRegister>(init)->init();
				expr->compile(vreg, init, init_scope);

				if (!(*expr_type && *type)) {
					if (expr_type->isInt() && type->isInt()) {
						// bool expr_is_signed = expr_type->isSigned(), var_is_signed = type->isSigned();
						auto expr_int = expr_type->ptrcast<IntType>(), var_int = type->ptrcast<IntType>();
						if (expr_type->isSigned() && type->isSigned()) {
							if (expr_int->width < var_int->width)
								init.add<SextInstruction>(vreg, vreg, var_int->width);
							else
								init.add<AndIInstruction>(vreg, vreg, int((1ul << var_int->width) - 1));
						} else if (var_int->width < expr_int->width)
							init.add<AndIInstruction>(vreg, vreg, int((1ul << var_int->width) - 1));
						// VregPtr vreg = std::make_shared<VirtualRegister>(init)->init();
						// if (expr_is_signed) {
						// 	auto expr_signed = expr_type->ptrcast<SignedType>();
						// 	if (var_is_signed) {
						// 		// signed -> signed
						// 		auto var_signed = type->ptrcast<SignedType>();
						// 		expr->compile(vreg, init, init_scope);
						// 		if (expr_signed->width < var_signed->width)
						// 			init.add<SextInstruction>(vreg, vreg, var_signed->width);
						// 		else
						// 			init.add<AndIInstruction>(vreg, vreg, (1ul << var_signed->width) - 1);
						// 	} else {
						// 		// signed -> unsigned
						// 		auto var_unsigned = type->ptrcast<UnsignedType>();
						// 		expr->compile(vreg, init, init_scope);
						// 		// TODO: verify
						// 		if (var_unsigned->width < expr_signed->width)
						// 			init.add<AndIInstruction>(vreg, vreg, (1ul << var_unsigned->width) - 1);
						// 	}
						// } else {
						// 	auto expr_unsigned = expr_type->ptrcast<UnsignedType>();
						// 	if (var_is_signed) {
						// 		// unsigned -> signed
						// 		auto var_signed = type->ptrcast<SignedType>();
						// 		expr->compile(vreg, init, init_scope);
						// 	} else {
						// 		// unsigned -> unsigned
						// 		auto var_unsigned = type->ptrcast<UnsignedType>();
						// 		expr->compile(vreg, init, init_scope);
						// 	}
						// }
					} else
						throw ImplicitConversionError(std::move(expr_type), std::move(type));
				}
				init.add<StoreIInstruction>(vreg, iter->first);
			}
		} else if (size == 1) {
			lines.push_back("\t%1b 0");
		} else if (size == 2) {
			lines.push_back("\t%2b 0");
		} else if (size == 4) {
			lines.push_back("\t%4b 0");
		} else {
			lines.push_back("\t%8b 0");
		}
	}

	for (auto &[name, function]: functions)
		function.compile();

	for (const auto &[str, id]: stringIDs) {
		lines.push_back("");
		lines.push_back("@.str" + std::to_string(id));
		lines.push_back("\t%stringz \"" + Util::escape(str) + "\"");
	}

	lines.push_back("");
	lines.push_back("%code");
	lines.push_back("");
	lines.push_back(":: .init");
	lines.push_back(":: main");
	lines.push_back("<halt>");

	for (const std::string &line:
		Util::split("|@.c|\t<prc $a0>|\t: $rt||@.ptr|\t<prx $a0>|\t: $rt||@.s|\t[$a0] -> $mf /b|\t: _strprint_print if "
		"$mf|\t: $rt|\t@_strprint_print|\t<prc $mf>|\t$a0++|\t: .s||@.s16|\t<prd $a0>|\t: $rt||@.s32|\t<prd $a0>|\t: $r"
		"t||@.s64|\t<prd $a0>|\t: $rt||@.s8|\t<prd $a0>|\t: $rt||@.u16|\t<prd $a0>|\t: $rt||@.u32|\t<prd $a0>|\t: $rt||"
		"@.u64|\t<prd $a0>|\t: $rt||@.u8|\t<prd $a0>|\t: $rt", "|", false))
		lines.push_back(line);

	for (auto &[name, function]: functions)
		if (name == ".init" || !function.isBuiltin()) {
			lines.push_back("");
			lines.push_back("@" + function.name);
			for (const std::string &line: function.stringify())
				lines.push_back("\t" + line);
		}
}

size_t Program::getStringID(const std::string &str) {
	if (stringIDs.count(str) != 0)
		return stringIDs.at(str);
	return stringIDs[str] = stringIDs.size();
}
