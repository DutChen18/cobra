#include "cobra/ringbuffer.hh"
#include <cassert>

int main() {
	using namespace cobra;

	{
		ringbuffer<int> a(1);

		a.push_back(42);
		assert(a.front() == 42);
	}
	{
		ringbuffer<int> a(2);

		a.push_back(42);
		assert(a.front() == 42);

		a.push_back(21);
		assert(a.front() == 42);

		a.push_back(11);
		assert(a.front() == 21);

		a.push_back(5);
		assert(a.front() == 11);
	}
}
