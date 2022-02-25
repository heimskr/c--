#include "ASTNode.h"
#include "Casting.h"
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

	for (const ASTNode *node: root)
		switch (node->symbol) {
			case CMMTOK_IDENT: {
				if (node->size() != 4)
					throw LocatedError(node->location, "Ident under program root not a function definition");
				const std::string &name = *node->text;
				if (out.signatures.count(name) != 0)
					throw LocatedError(node->location, "Cannot redefine function " + name);
				decltype(Signature::argumentTypes) args;
				for (const ASTNode *arg: *node->at(1))
					args.emplace_back(Type::get(*arg->front(), out));
				out.signatures.try_emplace(name, std::shared_ptr<Type>(Type::get(*node->at(0), out)), std::move(args));
				out.functions.try_emplace(name, out, node);
				break;
			}
			case CMM_DECL: { // Global variable
				const std::string &name = *node->at(1)->text;
				if (out.globals.count(name) != 0)
					throw LocatedError(node->location, "Cannot redefine global " + name);
				auto type = std::shared_ptr<Type>(Type::get(*node->at(0), out));
				if (node->size() <= 2)
					out.globalOrder.push_back(out.globals.try_emplace(name,
						std::make_shared<Global>(name, type, nullptr)).first);
				else
					out.globalOrder.push_back(out.globals.try_emplace(name, std::make_shared<Global>(name, type,
						std::shared_ptr<Expr>(Expr::get(*node->at(2), &init)))).first);
				break;
			}
			case CMMTOK_STRUCT: {
				const std::string &struct_name = *node->front()->text;
				if (node->size() == 1) {
					if (out.forwardDeclarations.count(struct_name) != 0)
						throw NameConflictError(struct_name, node->front()->location);
					out.forwardDeclarations.insert(struct_name);
				} else {
					std::vector<std::pair<std::string, TypePtr>> order;
					for (const ASTNode *field: *node->at(1))
						order.emplace_back(*field->text, Type::get(*field->front(), out));
					out.structs.emplace(struct_name, StructType::make(out, struct_name, order));
				}
				break;
			}
			case CMMTOK_META_NAME:
				out.name = *node->front()->text;
				break;
			case CMMTOK_META_AUTHOR:
				out.author = *node->front()->text;
				break;
			case CMMTOK_META_ORCID:
				out.orcid = *node->front()->text;
				break;
			case CMMTOK_META_VERSION:
				out.version = *node->front()->text;
				break;
			default:
				throw LocatedError(node->location, "Unexpected token under root: " +
					std::string(cmmParser.getName(node->symbol)));
		}

	// TODO: implement functions
	auto add_dummy = [&](const std::string &function_name) -> Function & {
		auto &fn = out.functions.try_emplace("`" + function_name, out, nullptr).first->second;
		fn.name = "`" + function_name;
		return fn;
	};
	add_dummy("s").setArguments({{"`string", std::make_shared<PointerType>(new UnsignedType(8))}});
	add_dummy("c").setArguments({{"`char", std::make_shared<UnsignedType>(8)}});
	add_dummy("ptr").setArguments({{"`pointer", std::make_shared<PointerType>(new VoidType())}});
	add_dummy("bool").setArguments({{"`boolean", std::make_shared<BoolType>()}});
	for (size_t i = 8; i <= 64; i *= 2) {
		add_dummy("s" + std::to_string(i)).setArguments({{"`int", std::make_shared<SignedType>(i)}});
		add_dummy("u" + std::to_string(i)).setArguments({{"`int", std::make_shared<UnsignedType>(i)}});
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
		const auto &global_name = iter->first;
		const auto &expr = iter->second->value;
		lines.push_back("");
		lines.push_back("@" + global_name);
		auto type = iter->second->type;
		auto size = type->getSize();
		if (expr) {
			auto value = expr->evaluate(init.selfScope);
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
				TypePtr expr_type = expr->getType(init.selfScope);
				VregPtr vreg = init.newVar();
				if (auto *initializer = expr->cast<InitializerExpr>()) {
					init.add<SetIInstruction>(vreg, global_name);
					initializer->fullCompile(vreg, init, init_scope);
				} else {
					expr->compile(vreg, init, init_scope);
					if (!tryCast(*expr_type, *type, vreg, init))
						throw ImplicitConversionError(expr_type, type);
					init.add<StoreIInstruction>(vreg, iter->first, size);
				}
			}
		} else if (size == 1) {
			lines.push_back("\t%1b 0");
		} else if (size == 2) {
			lines.push_back("\t%2b 0");
		} else if (size == 4) {
			lines.push_back("\t%4b 0");
		} else if (size == 8) {
			lines.push_back("\t%8b 0");
		} else {
			lines.push_back("\t%fill " + std::to_string(size) + " 0");
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
		Util::split("|@`c|\t<prc $a0>|\t: $rt||@`ptr|\t<prc '0'>|\t<prc 'x'>|\t<prx $a0>|\t: $rt||@`s|\t[$a0] -> $mf /b"
		"|\t: _strprint_print if $mf|\t: $rt|\t@_strprint_print|\t<prc $mf>|\t$a0++|\t: `s||@`s16|\tsext16 $a0 -> $a0|"
		"\t<prd $a0>|\t: $rt||@`s32|\tsext32 $a0 -> $a0|\t<prd $a0>|\t: $rt||@`s64|\t<prd $a0>|\t: $rt||@`s8|\tsext8 $a"
		"0 -> $a0|\t<prd $a0>|\t: $rt||@`u16|\t<prd $a0>|\t: $rt||@`u32|\t<prd $a0>|\t: $rt||@`u64|\t<prd $a0>|\t: $rt|"
		"|@`u8|\t<prd $a0>|\t: $rt||@`bool|\t!$a0 -> $a0|\t!$a0 -> $a0|\t<prd $a0>|\t: $rt", "|", false))
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
