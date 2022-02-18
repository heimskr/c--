#pragma once

#include <memory>

class Function;
struct Type;
struct VirtualRegister;

bool tryCast(std::shared_ptr<Type> expr_type, std::shared_ptr<Type> var_type, std::shared_ptr<VirtualRegister>,
             Function &);

void typeCheck(std::shared_ptr<Type> expr_type, std::shared_ptr<Type> var_type, std::shared_ptr<VirtualRegister>,
               Function &);
