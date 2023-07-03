#include "cobra/asyncio/stream_utils.hh"
#include "cobra/uri.hh"
#include "util/stringstream.hh"
#include "cobra/asyncio/future_task.hh"
#include "cobra/uri.hh"
#include <cassert>

int main() {
	using namespace cobra;
	auto text = block_task(make_adapter(uri::unescape_stream<test::istringstream>(test::istringstream("%61"))).collect());
	assert(text == "a");
}
