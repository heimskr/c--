#pragma once

#include <memory>
#include <set>

template <typename T>
struct WeakCompare {
	bool operator()(std::weak_ptr<T> left, std::weak_ptr<T> right) const {
		std::shared_ptr<T> llock = left.lock();
		std::shared_ptr<T> rlock = right.lock();
		if (!rlock)
			return false;
		if (!llock)
			return true;
		return llock.get() < rlock.get();
	}
};

template <typename T>
using WeakSet = std::set<std::weak_ptr<T>, WeakCompare<T>>;
