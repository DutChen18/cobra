#ifndef COBRA_THREAD_HH
#define COBRA_THREAD_HH

namespace cobra {
	template<class T>
	class atomic {
	private:
		T value;
	public:
		atomic() {
		}

		atomic(const T& value) {
			this->value = value;
		}

		operator T() const {
			return value;
		}

		atomic& operator=(const T& value) {
			this->value = value;
			return *this;
		}

		atomic& operator++() {
			++value;
			return *this;
		}

		atomic& operator--() {
			--value;
			return *this;
		}
	};
}

#endif
