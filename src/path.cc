#include "cobra/path.hh"

#include <stdexcept>

namespace cobra {
	path::path(const std::string& str) {
		std::size_t index = 0;
		type = path_type::relative;

		if (str.empty()) {
			throw std::runtime_error("empty path");
		}

		while (index < str.length() && str[index] == '/') {
			type = path_type::absolute;
			index += 1;
		}

		while (index < str.length()) {
			std::size_t start = index;

			while (index < str.length() && str[index] != '/') {
				index += 1;
			}

			components.push_back(str.substr(start, index - start));

			while (index < str.length() && str[index] == '/') {
				index += 1;
			}
		}
	}
	
	path::path(path_type type, std::vector<std::string> components) {
		this->type = type;
		this->components = components;
	}
	
	path::operator std::string() const {
		std::string result;
		std::string delim;

		if (type == path_type::relative && components.size() == 0) {
			return ".";
		}

		if (type == path_type::absolute) {
			result = "/";
		}

		for (const auto& component : components) {
			result += delim + component;
			delim = "/";
		}

		return result;
	}

	path path::normalize() const {
		std::vector<std::string> normalized;

		for (const auto& component : components) {
			if (component == "..") {
				if (!normalized.empty()) {
					normalized.pop_back();
				}
			} else if (component != ".") {
				normalized.push_back(component);
			}
		}

		return path(type, normalized);
	}

	path path::mount(const path& other) const {
		std::vector<std::string> mounted;
		std::vector<std::string> normalized = other.normalize().components;

		mounted.insert(mounted.end(), components.begin(), components.end());
		mounted.insert(mounted.end(), normalized.begin(), normalized.end());

		return path(type, mounted);
	}

	path path::dirname() const {
		std::vector<std::string> directory;

		if (components.empty()) {
			return *this;
		} else {
			directory.insert(directory.end(), components.begin(), components.end());
		}

		directory.pop_back();

		return path(type, directory);
	}

	std::string path::basename() const {
		if (components.empty()) {
			return "";
		} else {
			return components.back();
		}
	}

	bool path::empty() const {
		return components.empty();
	}
}

