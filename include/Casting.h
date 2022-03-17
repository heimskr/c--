#pragma once

#include <memory>

class Function;
struct ASTLocation;
struct Type;
struct VirtualRegister;

/** Tries to do an implicit cast if one is allowed and needed. Returns false if one is not allowed. */
bool tryCast(const Type &right_type, const Type &left_type, const std::shared_ptr<VirtualRegister> &, Function &,
             const ASTLocation &);

/** Throws an ImplicitConversionError if tryCast returns false for the given arguments. */
void typeCheck(const Type &right_type, const Type &left_type, const std::shared_ptr<VirtualRegister> &, Function &,
               const ASTLocation &);
