#pragma once

#include <memory>
#include <string>

class Function;
struct VirtualRegister;

class Allocator {
	protected:
		Function &function;
		int spillCount = 0;
		int attempts = 0;

	public:
		enum class Result: int {Spilled = 1, NotSpilled = 2, Success = 3};

		std::shared_ptr<VirtualRegister> lastSpill, lastSpillAttempt;

		Allocator(Function &function_): function(function_) {}
		virtual ~Allocator() {}

		virtual Result attempt() = 0;
		int getAttempts() const { return attempts; }
		int getSpillCount() const { return spillCount; }

		static std::string stringify(Result result) {
			return result == Result::Spilled? "Spilled" : (result == Result::NotSpilled? "NotSpilled" :
				(result == Result::Success? "Success" : "?"));
		}
};
