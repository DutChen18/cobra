#include "cobra/ringbuffer.hh"
#include <cassert>

int main() {
	using namespace cobra;

	{
		ringbuffer<int> a(1);

		assert(*a.push_back(42) == 42);
		assert(*a.push_back(21) == 21);
	}
	{
		ringbuffer<int> a(2);

		assert(*a.push_back(42) == 42);
		assert(*a.push_back(21) == 21);
		assert(*a.push_back(11) == 11);
	}
	{
		ringbuffer<int> a(1);

		a.push_back(1);
		assert(a[0] == 1);
		a.push_back(2);
		assert(a[0] == 2);
	}
	{
		ringbuffer<int> a(2);

		a.push_back(1);
		assert(a[0] == 1);

		a.push_back(2);
		assert(a[0] == 1);
		assert(a[1] == 2);

		a.push_back(3);
		assert(a[0] == 2);
		assert(a[1] == 3);
	}
}
