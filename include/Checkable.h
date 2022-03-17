#pragma once

struct Checkable {
	protected:
		Checkable() = default;

	public:
		Checkable(const Checkable &) = delete;
		Checkable(Checkable &&) = delete;
		Checkable & operator=(const Checkable &) = delete;
		Checkable & operator=(Checkable &&) = delete;
		virtual ~Checkable() = default;

		template <typename T>
		[[nodiscard]] bool is() const {
			return bool(dynamic_cast<const T *>(this));
		}

		template <typename T>
		[[nodiscard]] T * cast() {
			return dynamic_cast<T *>(this);
		}

		template <typename T>
		[[nodiscard]] const T * cast() const {
			return dynamic_cast<const T *>(this);
		}
};
