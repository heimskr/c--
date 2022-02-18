#pragma once

#include <memory>

class Function;
struct Type;
struct VirtualRegister;

/** Tries to do an implicit cast if one is allowed and needed. Returns false if one is not allowed. */
bool tryCast(const Type &expr_type, const Type &var_type, std::shared_ptr<VirtualRegister>, Function &);

/** Throws an ImplicitConversionError if tryCast returns false for the given arguments. */
void typeCheck(const Type &expr_type, const Type &var_type, std::shared_ptr<VirtualRegister>, Function &);
