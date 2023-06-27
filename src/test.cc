#include <ranges>

int main() {
	auto even = [](int i) { return i % 2 == 0; };
	auto square = [](int i) { return i * i; };
	auto h = std::views::filter(even) | std::views::transform(square);
}
