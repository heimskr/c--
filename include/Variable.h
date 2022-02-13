#pragma once

#include <memory>
#include <string>

class Function;

struct VirtualRegister {
	int id;
	int reg = -1;

	VirtualRegister(int id_): id(id_) {}
	VirtualRegister(Function &function);

	std::string regOrID() const;
};

using VregPtr = std::shared_ptr<VirtualRegister>;
