#include <format>

struct foo {};

template <>
class std::formatter<foo> {
public:
	constexpr auto parse(std::format_parse_context& fpc) {
		return fpc.begin();
	}

	auto format(foo ctrl, std::format_context& fc) const {
		return std::format_to(fc.out(), "");
	}
};

int main(int argc, char **argv) {
	std::format("{}", foo());
	return 0;
}
