#include "ASTNode.h"
#include "Casting.h"
#include "Enums.h"
#include "Errors.h"
#include "Expr.h"
#include "Lexer.h"
#include "Parser.h"
#include "Program.h"
#include "Scope.h"
#include "Type.h"
#include "Why.h"
#include "WhyInstructions.h"

Program compileRoot(const ASTNode &root, const std::string &filename) {
	Program out(filename);

	FunctionPtr init = Function::make(out, nullptr);
	out.functions.emplace(".init", init);
	out.bareFunctions.emplace(".init", init);
	init->name = ".init";

	for (const ASTNode *node: root)
		switch (node->symbol) {
			case CPMTOK_IDENT: { // Function definition
				const size_t size = node->size();
				if (size < 4 || 6 < size)
					throw GenericError(node->location, "Ident under program root not a function definition");
				std::string name = *node->text;
				if (name == "$d")
					throw InvalidFunctionNameError(name, node->location);
				if (name == "~")
					name = "$d";
				FunctionPtr function = Function::make(out, node);
				function->name = name;
				if (size == 5 || size == 6) {
					// 0: return type
					// 1: args list
					// 2: fnattrs
					// 3: block
					// 4: struct name
					// 5: static
					const std::string &struct_name = *node->at(4)->text;
					if (out.structs.count(struct_name) == 0) {
						std::string message = "Can't define function ";
						message += struct_name;
						message += "::";
						message += name;
						message += ": struct not defined";
						throw GenericError(node->at(4)->location, message);
					}
					function->setStructParent(out.structs.at(struct_name), size == 6);
					if (function->name == "$d") {
						if (auto destructor = function->structParent->destructor.lock())
							if (destructor->source != nullptr)
								throw GenericError(node->location, "Struct " + struct_name + " cannot have multiple "
									"destructors");
						function->structParent->destructor = function;
					}
				}
				const std::string mangled = function->mangle();
				if (out.functions.count(mangled) != 0)
					throw GenericError(node->location, "Cannot redefine function " + mangled);
				Types args;
				for (const ASTNode *arg: *node->at(1))
					args.emplace_back(Type::get(*arg->front(), out));
				out.signatures.try_emplace(mangled, TypePtr(Type::get(*node->front(), out)), std::move(args));
				out.functions.emplace(mangled, function);
				out.bareFunctions.emplace(name, function);
				break;
			}
			case CPMTOK_OPERATOR: { // Operator overload definition
				// 0: return type
				// 1: operator type
				// 2: args list
				// 3: fnattrs
				// 4: block
				FunctionPtr function = Function::make(out, node);
				const std::string mangled = function->mangle();
				if (out.functions.count(mangled) != 0)
					throw GenericError(node->location, "Cannot redefine operator " + mangled);
				Types args;
				for (const ASTNode *arg: *node->at(2))
					args.emplace_back(Type::get(*arg->front(), out));
				out.signatures.try_emplace(mangled, TypePtr(Type::get(*node->front(), out)), std::move(args));
				out.functions.emplace(mangled, function);
				out.bareFunctions.emplace(function->name, function);
				out.operators.emplace(node->at(1)->symbol, function);
				break;
			}
			case CPMTOK_PLUS: { // Constructor definition
				// 0: struct name
				// 1: args list
				// 2: fnattrs
				// 3: block
				const std::string &struct_name = *node->front()->text;
				if (out.structs.count(struct_name) == 0)
					throw GenericError(node->front()->location, "Can't define constructor for " + struct_name +
						": struct not defined");
				auto struct_type = out.structs.at(struct_name);
				Types args;
				for (const ASTNode *arg: *node->at(1))
					args.emplace_back(Type::get(*arg->front(), out));
				FunctionPtr fn = Function::make(out, node);
				fn->setStructParent(struct_type, false);
				fn->structParent->constructors.insert(fn);
				const std::string mangled = fn->mangle();
				if (out.functions.count(mangled) != 0)
					throw GenericError(node->location, "Cannot redefine constructor " + mangled);
				out.signatures.try_emplace(mangled, struct_type, std::move(args));
				out.functions.emplace(mangled, fn);
				out.bareFunctions.emplace(fn->name, fn);
				break;
			}
			case CPM_FNDECL: {
				const std::string &name = *node->text;
				decltype(Signature::argumentTypes) args;
				for (const ASTNode *arg: *node->at(1))
					args.emplace_back(Type::get(*arg->front(), out));
				FunctionPtr fn = Function::make(out, node);
				const std::string mangled = fn->mangle();
				if (out.signatures.count(mangled) != 0)
					throw GenericError(node->location, "Cannot redefine function " + mangled);
				out.signatures.try_emplace(mangled, TypePtr(Type::get(*node->front(), out)), std::move(args));
				out.functionDeclarations.emplace(mangled, fn);
				out.bareFunctionDeclarations.emplace(name, fn);
				break;
			}
			case CPM_DECL: { // Global variable
				const std::string &name = *node->at(1)->text;
				if (out.globals.count(name) != 0)
					throw GenericError(node->location, "Cannot redefine global " + name);
				auto type = TypePtr(Type::get(*node->front(), out));
				if (node->size() <= 2)
					out.globalOrder.push_back(out.globals.try_emplace(name,
						std::make_shared<Global>(name, type, nullptr)).first);
				else
					out.globalOrder.push_back(out.globals.try_emplace(name, std::make_shared<Global>(name, type,
						std::shared_ptr<Expr>(Expr::get(*node->at(2), init.get())))).first);
				break;
			}
			case CPMTOK_STRUCT: {
				const std::string &struct_name = *node->front()->text;
				if (node->size() == 1) {
					if (out.forwardDeclarations.count(struct_name) != 0)
						throw NameConflictError(struct_name, node->front()->location);
					out.forwardDeclarations.insert(struct_name);
				} else {
					std::vector<std::pair<std::string, TypePtr>> order;
					std::map<std::string, TypePtr> statics;
					std::unordered_set<std::string> field_names;
					for (const ASTNode *child: *node->at(1))
						if (child->symbol == CPMTOK_IDENT) {
							const std::string &field_name = *child->text;
							if (field_names.count(field_name) != 0)
								throw NameConflictError(field_name, child->location);
							field_names.insert(field_name);
							if (child->attributes.count("static") == 0) {
								order.emplace_back(field_name, Type::get(*child->front(), out));
							} else {
								if (out.globals.count(field_name) != 0)
									throw NameConflictError(field_name, child->location);
								auto type = TypePtr(Type::get(*child->front(), out));
								const std::string mangled = Util::mangleStaticField(struct_name, type, field_name);
								statics.emplace(field_name, type);
								GlobalPtr global;
								if (child->size() == 1) {
									global = std::make_shared<Global>(mangled, type, nullptr);
								} else {
									auto expr = ExprPtr(Expr::get(*child->at(1), init.get()));
									global = std::make_shared<Global>(mangled, type, expr);
								}
								out.globalOrder.push_back(out.globals.emplace(mangled, global).first);
							}
						}
					auto struct_type = out.structs.emplace(struct_name, StructType::make(out, struct_name,
						std::move(order), std::move(statics))).first->second;
					for (ASTNode *child: *node->at(1))
						if (child->symbol == CPM_FNDECL) {
							const std::string &name = *child->text;
							decltype(Signature::argumentTypes) args;
							for (const ASTNode *arg: *child->at(1))
								args.emplace_back(Type::get(*arg->front(), out));
							FunctionPtr fn = Function::make(out, child);
							fn->structParent = struct_type;
							fn->setStatic(child->attributes.count("static") != 0);
							const std::string mangled = fn->mangle();
							if (out.signatures.count(mangled) != 0)
								throw GenericError(child->location, "Cannot redefine function " + mangled);
							TypePtr ret_type = TypePtr(Type::get(*child->front(), out));
							out.signatures.try_emplace(mangled, ret_type, std::move(args));
							out.functionDeclarations.emplace(mangled, fn);
							out.bareFunctionDeclarations.emplace(name, fn);
						} else if (child->symbol == CPM_CONSTRUCTORDECL) {
							// 0: args
							// 1: fnattrs
							decltype(Signature::argumentTypes) args;
							for (const ASTNode *arg: *child->at(0))
								args.emplace_back(Type::get(*arg->front(), out));
							child->map["StructName"] = struct_type->name;
							FunctionPtr fn = Function::make(out, child);
							fn->structParent = struct_type;
							fn->name = "$c";
							fn->setStructParent(struct_type, false);
							fn->structParent->constructors.insert(fn);
							const std::string mangled = fn->mangle();
							if (out.functions.count(mangled) != 0)
								throw GenericError(node->location, "Cannot redefine constructor " + mangled);
							out.signatures.try_emplace(mangled, struct_type, std::move(args));
							out.functionDeclarations.emplace(mangled, fn);
							out.bareFunctionDeclarations.emplace(fn->name, fn);
						} else if (child->symbol == CPMTOK_TILDE) {
							FunctionPtr fn = Function::make(out, nullptr);
							fn->structParent = struct_type;
							fn->structParent->destructor = fn;
							fn->name = "$d";
							fn->setStatic(false);
							const std::string mangled = fn->mangle();
							if (out.signatures.count(mangled) != 0)
								throw GenericError(child->location, "Cannot redefine function " + mangled);
							out.signatures.try_emplace(mangled, VoidType::make(), Types());
							out.functionDeclarations.emplace(mangled, fn);
							out.bareFunctionDeclarations.emplace("$d", fn);
						} else if (child->symbol != CPMTOK_IDENT)
							child->debug();
				}
				break;
			}
			case CPMTOK_META_NAME:
				out.name = *node->front()->text;
				break;
			case CPMTOK_META_AUTHOR:
				out.author = *node->front()->text;
				break;
			case CPMTOK_META_ORCID:
				out.orcid = *node->front()->text;
				break;
			case CPMTOK_META_VERSION:
				out.version = *node->front()->text;
				break;
			default:
				throw GenericError(node->location, "Unexpected token under root: " +
					std::string(cpmParser.getName(node->symbol)));
		}

	auto add_dummy = [&](const std::string &function_name) -> Function & {
		FunctionPtr fn = Function::make(out, nullptr);
		out.functions.emplace("`" + function_name, fn);
		out.bareFunctions.emplace("`" + function_name, fn);
		fn->name = "`" + function_name;
		return *fn;
	};
	add_dummy("s").setArguments({{"`string", std::make_shared<PointerType>((new UnsignedType(8))->setConst(true))}});
	add_dummy("c").setArguments({{"`char", std::make_shared<UnsignedType>(8)}});
	add_dummy("ptr").setArguments({{"`pointer", std::make_shared<PointerType>((new VoidType)->setConst(true))}});
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
		lines.emplace_back("name: " + name);
	if (!author.empty())
		lines.emplace_back("author: " + author);
	if (!orcid.empty())
		lines.emplace_back("orcid: " + orcid);
	if (!version.empty())
		lines.emplace_back("version: " + version);
	lines.emplace_back("");
	lines.emplace_back("#text");
	lines.emplace_back("");
	lines.emplace_back("%data");
	auto init_scope = std::make_shared<GlobalScope>(*this);

	auto &init = functions.at(".init");

	for (const auto &iter: globalOrder) {
		const auto &global_name = iter->first;
		const auto &expr = iter->second->value;
		lines.emplace_back("");
		lines.emplace_back("@" + global_name);
		auto type = iter->second->getType();
		auto size = type->getSize();
		if (expr) {
			auto value = expr->evaluate(Context(*this, init->selfScope));
			if (value && size == 1) {
				lines.emplace_back("\t%1b " + std::to_string(*value));
			} else if (value && size == 2) {
				lines.emplace_back("\t%2b " + std::to_string(*value));
			} else if (value && size == 4) {
				lines.emplace_back("\t%4b " + std::to_string(*value));
			} else if (value && size == 8) {
				lines.emplace_back("\t%8b " + std::to_string(*value));
			} else {
				lines.emplace_back("\t%fill " + std::to_string(size) + " 0");
				TypePtr expr_type = expr->getType(Context(*this, init->selfScope));
				VregPtr vreg = init->newVar();
				if (auto *initializer = expr->cast<InitializerExpr>()) {
					init->add<SetIInstruction>(vreg, TypedImmediate(OperandType(*iter->second->getType()),
						global_name));
					initializer->fullCompile(vreg, *init, Context(*this, init_scope));
				} else {
					expr->compile(vreg, *init, Context(*this, init_scope), 1);
					if (!tryCast(*expr_type, *type, vreg, *init, expr->getLocation()))
						throw ImplicitConversionError(expr_type, type, expr->getLocation());
					OperandType op_type(*iter->second->getType());
					++op_type.pointerLevel;
					init->add<StoreIInstruction>(vreg, TypedImmediate(op_type, iter->first))->setDebug(*expr);
				}
			}
		} else if (size == 1) {
			lines.emplace_back("\t%1b 0");
		} else if (size == 2) {
			lines.emplace_back("\t%2b 0");
		} else if (size == 4) {
			lines.emplace_back("\t%4b 0");
		} else if (size == 8) {
			lines.emplace_back("\t%8b 0");
		} else {
			lines.emplace_back("\t%fill " + std::to_string(size) + " 0");
		}
	}

	for (auto &[name, function]: functions)
		function->compile();

	for (const auto &[str, id]: stringIDs) {
		lines.emplace_back("");
		lines.emplace_back("@.str" + std::to_string(id));
		lines.emplace_back("\t%stringz \"" + Util::escape(str) + "\"");
	}

	lines.emplace_back("");
	lines.emplace_back("%code");
	lines.emplace_back("");
	lines.emplace_back(":: .init");
	lines.emplace_back(":: main");
	lines.emplace_back("<halt>");

	for (const std::string &line:
		Util::split("|@`c|\t<prc $a0{uc}>|\t: $rt{v*}||@`ptr|\t<prc '0'>|\t<prc 'x'>|\t<prx $a0{v}>|\t: $rt{v*}||@`s|\t"
			"[$a0{uc*}] -> $mf{uc}|\t: _strprint_print if $mf{uc}|\t: $rt{v*}|\t@_strprint_print|\t<prc $mf{uc}>|\t$a0{"
			"uc*}++|\t: `s||@`s16|\t<prd $a0{ss}>|\t: $rt{v*}||@`s32|\t<prd $a0{si}>|\t: $rt{v*}||@`s64|\t<prd $a0{sl}>"
			"|\t: $rt{v*}||@`s8|\t<prd $a0{sc}>|\t: $rt{v*}||@`u16|\t<prd $a0{us}>|\t: $rt{v*}||@`u32|\t<prd $a0{ui}>|"
			"\t: $rt{v*}||@`u64|\t<prd $a0{ul}>|\t: $rt{v*}||@`u8|\t<prd $a0{uc}>|\t: $rt{v*}||@`bool|\t!$a0{v} -> $a0{"
			"uc}|\t!$a0{uc} -> $a0{uc}|\t<prd $a0{uc}>|\t: $rt{v*}", "|", false))
		lines.emplace_back(line);

	std::map<DebugData, size_t> debug_map;
	std::map<size_t, DebugData *> inverse_debug_map;

	const size_t offset = 1 + functions.size();

	for (const auto &[name, function]: functions)
		for (const auto &instruction: function->instructions)
			if (instruction->debug)
				if (debug_map.count(instruction->debug) == 0) {
					const size_t index = offset + debug_map.size();
					debug_map.emplace(instruction->debug, index);
					inverse_debug_map.emplace(index, &instruction->debug);
				}

	for (auto &[name, function]: functions)
		if (name == ".init" || !function->isBuiltin()) {
			lines.emplace_back("");
			lines.emplace_back("@" + function->mangle());
			for (const std::string &line: function->stringify(debug_map))
				lines.emplace_back("\t" + line);
		}

	lines.emplace_back("");
	lines.emplace_back("#debug");
	lines.emplace_back("");
	lines.emplace_back("1 \"" + Util::escape(filename) + "\"");
	std::map<std::string, size_t> function_indices;
	for (auto &[name, function]: functions) {
		const std::string mangled = function->mangle();
		function_indices.emplace(mangled, function_indices.size() + 1);
		lines.emplace_back("2 \"" + Util::escape(mangled) + "\"");
	}

	for (const auto &[index, debug]: inverse_debug_map)
		lines.emplace_back("3 0 " + std::to_string(debug->location.line + 1) + " " +
			std::to_string(debug->location.column) + " " + std::to_string(function_indices.at(debug->mangledFunction)));
}

size_t Program::getStringID(const std::string &str) {
	if (stringIDs.count(str) != 0)
		return stringIDs.at(str);
	const size_t old_size = stringIDs.size();
	return stringIDs[str] = old_size;
}

FunctionPtr Program::getOperator(const std::vector<Type *> &types, int oper, const ASTLocation &location) const {
	if (operators.count(oper) == 0)
		return nullptr;

	std::vector<FunctionPtr> candidates;
	const size_t types_size = types.size();

	auto [begin, end] = operators.equal_range(oper);
	for (auto iter = begin; iter != end; ++iter)
		if (iter->second->arguments.size() == types_size) {
			bool good = true;
			for (size_t i = 0; i < types_size; ++i) {
				Type *type = types.at(i);
				if (!(*type && *iter->second->getArgumentType(i))) {
					good = false;
					break;
				}
			}
			if (good)
				candidates.push_back(iter->second);
		}

	if (candidates.empty())
		return nullptr;

	if (candidates.size() == 1)
		return candidates.front();

	std::stringstream err;
	err << "Multiple results found for " << Util::getOperator(oper) << '(';
	for (size_t i = 0, max = types.size(); i < max; ++i) {
		if (i != 0)
			err << ", ";
		err << *types.at(i);
	}
	err << ')';
	throw AmbiguousError(location, err.str());
}
