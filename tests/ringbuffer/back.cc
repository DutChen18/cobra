#include "cobra/ringbuffer.hh"
#include <cassert>

int main() {
	using namespace cobra;

	{
		ringbuffer<int> a(1);

		a.push_back(42);
		assert(a.back() == 42);
	}
	{
		ringbuffer<int> a(2);

		a.push_back(42);
		assert(a.back() == 42);

		a.push_back(21);
		assert(a.back() == 21);

		a.push_back(11);
		assert(a.back() == 11);

		a.push_back(5);
		assert(a.back() == 5);
	}
}
