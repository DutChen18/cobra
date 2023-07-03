#include "cobra/uri.hh"
#include <cassert>

int main() {
	using namespace cobra::uri;
	assert(hex_to_byte('0') == 0);
	assert(hex_to_byte('1') == 1);
	assert(hex_to_byte('2') == 2);
	assert(hex_to_byte('3') == 3);
	assert(hex_to_byte('4') == 4);
	assert(hex_to_byte('5') == 5);
	assert(hex_to_byte('6') == 6);
	assert(hex_to_byte('7') == 7);
	assert(hex_to_byte('8') == 8);
	assert(hex_to_byte('9') == 9);
	assert(hex_to_byte('A') == 10);
	assert(hex_to_byte('B') == 11);
	assert(hex_to_byte('C') == 12);
	assert(hex_to_byte('D') == 13);
	assert(hex_to_byte('E') == 14);
	assert(hex_to_byte('F') == 15);
	assert(hex_to_byte('a') == 10);
	assert(hex_to_byte('b') == 11);
	assert(hex_to_byte('c') == 12);
	assert(hex_to_byte('d') == 13);
	assert(hex_to_byte('e') == 14);
	assert(hex_to_byte('f') == 15);
}
