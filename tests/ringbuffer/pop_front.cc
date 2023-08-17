#include "cobra/ringbuffer.hh"
#include <cassert>

int main() {
	using namespace cobra;

	{
		ringbuffer<int> a(1);

		a.push_back(1);
		assert(a.size() == 1);
		assert(a.pop_front() == 1);
		assert(a.empty());
	}
	{
		ringbuffer<int> a(2);

		a.push_back(1);
		a.push_back(2);
		a.push_back(3);
		assert(a.size() == 2);
		assert(a.pop_front() == 2);
		assert(a.size() == 1);

		assert(a.pop_front() == 3);
		assert(a.empty());
	}
}
