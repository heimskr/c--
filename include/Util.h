#pragma once

#include <string>
#include <vector>

namespace Util {
	std::vector<std::string> split(const std::string &str, const std::string &delimiter, bool condense = true);
	long parseLong(const std::string &, int base = 10);
	long parseLong(const std::string *, int base = 10);
	long parseLong(const char *, int base = 10);
	std::string read(const std::string &path);
}
