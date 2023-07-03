#include "cobra/asyncio/stream.hh"
#include "cobra/uri.hh"
#include "util/stringstream.hh"
#include "cobra/asyncio/future_task.hh"
#include "util/assert.hh"

cobra::uri::segment test_str(const std::string& input) {
	auto stream = test::istringstream(input);
	return block_task(cobra::uri::segment::parse(stream));
}

int main() {
	test_str("abc");
}
