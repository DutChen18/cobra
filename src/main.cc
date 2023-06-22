#include "cobra/asyncio/task.hh"

#include <iostream>
#include <format>
#include <cstdlib>

cobra::task<int> test() {
	co_return 1;
}

int main() {
	cobra::task<int> task = test();
	cobra::future<int>& future = task.operator co_await();

	std::cout << std::format("{}", future.await_resume()) << std::endl;

	return EXIT_SUCCESS;
}
