#include <fstream>
#include <stdexcept>

#include "Util.h"

namespace Util {
	std::vector<std::string> split(const std::string &str, const std::string &delimiter, bool condense) {
		if (str.empty())
			return {};

		size_t next = str.find(delimiter);
		if (next == std::string::npos)
			return {str};

		std::vector<std::string> out {};
		const size_t delimiter_length = delimiter.size();
		size_t last = 0;

		out.push_back(str.substr(0, next));

		while (next != std::string::npos) {
			last = next;
			next = str.find(delimiter, last + delimiter_length);
			std::string sub = str.substr(last + delimiter_length, next - last - delimiter_length);
			if (!sub.empty() || !condense)
				out.push_back(std::move(sub));
		}

		return out;
	}

	long parseLong(const std::string &str, int base) {
		const char *c_str = str.c_str();
		char *end;
		long parsed = strtol(c_str, &end, base);
		if (c_str + str.length() != end)
			throw std::invalid_argument("Not an integer: \"" + str + "\"");
		return parsed;
	}

	long parseLong(const std::string *str, int base) {
		return parseLong(*str, base);
	}

	long parseLong(const char *str, int base) {
		return parseLong(std::string(str), base);
	}

	std::string read(const std::string &path) {
		std::ifstream file(path);
		if (!file.is_open())
			throw std::runtime_error("Couldn't open file for reading");
		std::string out;
		file.seekg(0, std::ios::end);
		out.reserve(file.tellg());
		file.seekg(0, std::ios::beg);
		out.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		return out;
	}
}
