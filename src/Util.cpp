#include <climits>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Enums.h"
#include "Type.h"
#include "Util.h"

namespace Util {
	std::string getOperator(int oper) {
		return "$o" + operator_map.at(oper);
	}

	std::string mangleStaticField(const std::string &struct_name, TypePtr type, const std::string &field) {
		std::stringstream out;
		out << ".f" << struct_name.size() << struct_name << type->mangle() << field.size() << field;
		return out.str();
	}

	std::string getSignature(std::shared_ptr<Type> ret, const std::vector<std::shared_ptr<Type>> &args) {
		std::stringstream out;
		if (ret)
			out << *ret;
		else
			out << '?';
		out << '(';
		bool first = true;
		for (const auto &arg: args) {
			if (first)
				first = false;
			else
				out << ", ";
			out << *arg;
		}
		out << ")";
		return out.str();
	}

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
		long parsed;
		size_t length;
		if (3 <= str.size() && str[0] == '0' && str[1] == 'x') {
			length = str.size() - 2;
			c_str += 2;
			parsed = strtol(c_str, &end, 16);
		} else {
			length = str.size();
			parsed = strtol(c_str, &end, base);
		}
		if (c_str + length != end)
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

	std::string escape(const std::string &str) {
		std::stringstream out;
		for (char ch: str) {
			if (ch == '"' || ch == '\\')
				out << '\\' << ch;
			else if (ch == '\t')
				out << "\\t";
			else if (ch == '\n')
				out << "\\n";
			else if (ch == '\r')
				out << "\\r";
			else if (ch == '\e')
				out << "\\e";
			else if (ch == '\a')
				out << "\\a";
			else if (ch == '\b')
				out << "\\b";
			else if (ch == '\0')
				out << "\\0";
			else
				out << ch;
		}
		return out.str();
	}

	bool inRange(ssize_t value) {
		return INT_MIN <= value && value <= INT_MAX;
	}

	std::string unescape(const std::string &str) {
		const size_t size = str.size();
		if (size == 0)
			return "";
		std::stringstream out;
		for (size_t i = 0; i < size; ++i) {
			char ch = str[i];
			if (ch == '\\') {
				if (i == size - 1)
					throw std::runtime_error("Backslash at end of string");
				switch (str[++i]) {
					case 'n':  out << '\n'; break;
					case 'r':  out << '\r'; break;
					case 'a':  out << '\a'; break;
					case 't':  out << '\t'; break;
					case 'b':  out << '\b'; break;
					case 'e':  out << '\e'; break;
					case '0':  out << '\0'; break;
					case '\\': out << '\\'; break;
					case '"':  out << '"';  break;
					case 'x': {
						if (size <= i + 2)
							throw std::runtime_error("Hexadecimal escape is too close to end of string");
						const char first = str[++i], second = str[++i];
						if (!isxdigit(first) || !isxdigit(second))
							throw std::runtime_error(std::string("Invalid hexadecimal escape: \\x") + first + second);
						out << char(strtol((std::string(1, first) + second).c_str(), nullptr, 16));
						break;
					}
					default: throw std::runtime_error("Unrecognized character: \\" + std::string(1, str[i]));
				}
			} else
				out << ch;
		}
		return out.str();
	}

	void validateSize(size_t size) {
		if (size != 1 && size != 2 && size != 4 && size != 8)
			throw std::runtime_error("Invalid size: " + std::to_string(size));
	}
}
