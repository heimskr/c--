#pragma once

#include <cstddef>
#include <functional>

template <typename T>
struct Hash {
	size_t operator()(const T &data) const {
		size_t out = 0xcbf29ce484222325ul;
		const auto *base = reinterpret_cast<const uint8_t *>(&data);
		for (size_t i = 0; i < sizeof(T); ++i)
			out = (out * 0x00000100000001b3) ^ base[i];
		return out;
	}
};

#define DEFHASH(t) namespace std { template <> class hash<t>: public Hash<t> {}; }
