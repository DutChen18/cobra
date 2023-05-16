#ifndef COBRA_PATH_HH
#define COBRA_PATH_HH

#include <vector>
#include <string>

namespace cobra {
	enum class path_type {
		absolute,
		relative,
	};

	class path {
	private:
		path_type type;
		std::vector<std::string> components;
	public:
		path(const std::string& str);
		path(path_type type, std::vector<std::string> components);

		operator std::string() const;

		path normalize() const;
		path mount(const path& other) const;
	};
};

#endif
