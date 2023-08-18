#include "cobra/ringbuffer.hh"
#include <cassert>

int main() {
	using namespace cobra;

	{
		ringbuffer<int> a(10);

		assert(a.begin() == a.end());

		for (int i = 0; i < 10; ++i) {
			assert(a.end() >= a.begin());
			a.push_back(i);
			assert(a.end() > a.begin());
			assert(a.end() - a.begin() == (i + 1));
		}

		assert(a.end() - a.begin() == 10);
		assert(a.begin() - a.end() == -10);
		assert(a.begin() < a.end());
		assert(a.end() > a.begin());

		for (int i = 0; i < 5; ++i) {
			a.push_back(i);
		}

		assert(a.end() - a.begin() == 10);
		assert(a.begin() - a.end() == -10);
		assert(a.begin() < a.end());
		assert(a.end() > a.begin());

		for (unsigned i = 0; i < 10; ++i) {
			assert(a.begin() + i < a.end());
		}
	}
}
