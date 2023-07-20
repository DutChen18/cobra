#include "cobra/http/uri.hh"
#include "cobra/http/util.hh"

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

	std::string uri_abs_path::string() const {
		std::string result;

		for (const uri_segment& segment : _segments) {
			result += "/" + hexify(segment, is_uri_segment);
		}

		return result;
	};

	uri_origin::uri_origin(uri_abs_path path, std::optional<uri_query> query) : _path(std::move(path)), _query(std::move(query)) {
	}

	const uri_abs_path& uri_origin::path() const {
		return _path;
	}

	const std::optional<uri_query>& uri_origin::query() const {
		return _query;
	}

	std::string uri_origin::string() const {
		if (auto query = _query) {
			return _path.string() + "?" + hexify(*query, is_uri_query);
		} else {
			return _path.string();
		}
	}

	uri_absolute::uri_absolute(std::string string) : _string(std::move(string)) {
	}

	std::string uri_absolute::string() const {
		return _string;
	}

	uri_authority::uri_authority(std::string string) : _string(std::move(string)) {
	}

	std::string uri_authority::string() const {
		return _string;
	}

	std::string uri_asterisk::string() const {
		return "*";
	}

	std::string uri::string() const {
		return std::visit([](auto value) { return value.string(); }, _variant);
	}
}
