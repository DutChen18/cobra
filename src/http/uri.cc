#include "cobra/http/uri.hh"

namespace cobra {
	uri_abs_path::uri_abs_path(std::vector<uri_segment> segments) : _segments(std::move(segments)) {
	}

	const std::vector<uri_segment>& uri_abs_path::segments() const {
		return _segments;
	}

	std::optional<std::filesystem::path> uri_abs_path::path() const {
		std::filesystem::path result;

		for (const uri_segment& segment : _segments) {
			if (segment.find('/') != uri_segment::npos) {
				return std::nullopt;
			} else {
				result /= segment;
			}
		}

		return result;
	}

	uri_abs_path uri_abs_path::normalize() const {
		std::vector<uri_segment> segments;

		for (const uri_segment& segment : _segments) {
			if (segment == "..") {
				if (!segments.empty()) {
					segments.pop_back();
				}
			} else if (segment != ".") {
				segments.push_back(segment);
			}
		}

		return segments;
	}

	uri_origin::uri_origin(uri_abs_path path, std::optional<uri_query> query) : _path(std::move(path)), _query(std::move(query)) {
	}

	const uri_abs_path& uri_origin::path() const {
		return _path;
	}

	const std::optional<uri_query>& uri_origin::query() const {
		return _query;
	}
}
