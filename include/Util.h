#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace Util {
	std::vector<std::string> split(const std::string &str, const std::string &delimiter, bool condense = true);
	long parseLong(const std::string &, int base = 10);
	long parseLong(const std::string *, int base = 10);
	long parseLong(const char *, int base = 10);
	std::string read(const std::string &path);
	std::string escape(const std::string &);
	bool inRange(ssize_t);
	std::string unescape(const std::string &);

	template <template <typename...> typename C, typename T, typename D>
	std::string join(const C<T> &container, D &&delimiter) {
		std::stringstream ss;
		bool first = true;
		for (const T &item: container) {
			if (first)
				first = false;
			else
				ss << delimiter;
			ss << item;
		}
		return ss.str();
	}

	template <typename T>
	std::string hex(T n) {
		std::stringstream ss;
		ss << std::hex << n;
		return ss.str();
	}

	template <template <typename...> typename C, typename T>
	C<T> combine(const C<T> &left, const C<T> &right) {
		C<T> out;
		for (const C<T> &container: {left, right})
			for (const T &item: container)
				out.push_back(item);
		return out;
	}

	template <typename T, template <typename...> typename C>
	bool equal(const C<T> &first, const C<T> &second) {
		if (first.size() != second.size())
			return false;
		for (const T &item: first)
			if (second.count(item) == 0)
				return false;
		return true;
	}
}
