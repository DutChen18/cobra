#include "cobra/ringbuffer.hh"
#include <cassert>

int main() {
	using namespace cobra;

	ringbuffer<int> empty(10);
	assert(empty.empty());
	assert(empty.size() == 0);
	assert(empty.capacity() == 10);
}
