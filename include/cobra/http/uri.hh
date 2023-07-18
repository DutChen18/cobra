#ifndef COBRA_HTTP_URI_HH
#define COBRA_HTTP_URI_HH

#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace cobra {
	using uri_segment = std::string;
	using uri_query = std::string;

	class uri_abs_path {
		std::vector<uri_segment> _segments;

	public:
		uri_abs_path(std::vector<uri_segment> segments);

		const std::vector<uri_segment>& segments() const;
		std::optional<std::filesystem::path> path() const;
		uri_abs_path normalize() const;
	};

	class uri_origin {
		uri_abs_path _path;
		std::optional<uri_query> _query;

	public:
		uri_origin(uri_abs_path path, std::optional<uri_query> query);

		const uri_abs_path& path() const;
		const std::optional<uri_query>& query() const;
	};
};

#endif
