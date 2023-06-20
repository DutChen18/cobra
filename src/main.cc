#include "cobra/asyncio/task.hh"

#include <iostream>
#include <format>
#include <cstdlib>

cobra::task<int> test2() {
	co_return 1;
}

cobra::task<int> test() {
	co_return co_await test2();
}

int main() {
	cobra::task<int> task = test();

	task.resume();
	std::cout << std::format("{}", task.promise().value()) << std::endl;;
	task.destroy();

	return EXIT_SUCCESS;
}
