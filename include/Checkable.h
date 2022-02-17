#pragma once

struct Checkable {
	virtual ~Checkable() {}

	template <typename T>
	bool is() const {
		return bool(dynamic_cast<const T *>(this));
	}

	template <typename T>
	T * cast() {
		return dynamic_cast<T *>(this);
	}

	template <typename T>
	const T * cast() const {
		return dynamic_cast<const T *>(this);
	}
};
