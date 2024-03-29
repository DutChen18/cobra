#ifndef COBRA_HTTP_URI_HH
#define COBRA_HTTP_URI_HH

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cobra {
	using uri_segment = std::string;
	using uri_query = std::string;

	class uri_abs_path : public std::vector<uri_segment> {
	public:
		uri_abs_path() = default;
		uri_abs_path(const std::filesystem::path& path);
		uri_abs_path(std::vector<uri_segment> segments);

		inline const std::vector<uri_segment>& segments() const {
			return *this;
		}
		std::optional<std::filesystem::path> path() const;
		uri_abs_path normalize() const;
		std::string string() const;
	};

	class uri_origin {
		uri_abs_path _path;
		std::optional<uri_query> _query;

	public:
		uri_origin(uri_abs_path path, std::optional<uri_query> query);

		const uri_abs_path& path() const;
		const std::optional<uri_query>& query() const;
		std::string string() const;
	};

	class uri_absolute {
		std::string _string;

	public:
		uri_absolute(std::string string);

		std::string string() const;
	};

	class uri_authority {
		std::string _string;

	public:
		uri_authority(std::string string);

		std::string string() const;
	};

	class uri_asterisk {
	public:
		std::string string() const;
	};

	class uri {
		std::variant<uri_origin, uri_absolute, uri_authority, uri_asterisk> _variant;

	public:
		template <class T>
		uri(T&& value) : _variant(std::forward<T>(value)) {}

		std::string string() const;

		template <class T>
		const T* get() const {
			return std::get_if<T>(&_variant);
		}
	};
}; // namespace cobra

#endif
