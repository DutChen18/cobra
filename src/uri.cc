#include "cobra/uri.hh"

#include "cobra/asyncio/stream_utils.hh"
#include "cobra/parse_utils.hh"

#include <cctype>
#include <vector>

namespace cobra {

	namespace uri {

		unsigned char hex_to_byte(int ch) {
			if (is_digit(ch)) {
				return ch - '0';
			} else {
				return std::tolower(ch) - 'a' + 10;
			}
		}

		scheme::scheme(std::string scheme) : _scheme(std::move(scheme)) {}

		task<scheme> scheme::parse(buffered_istream &stream) {
			if (is_alpha(co_await assert_get(stream))) {
				co_return scheme(co_await wrap_adapter(stream)
									 .take_while([](int ch) {
										 return is_alpha(ch) || is_digit(ch) || ch == '+' || ch == '-' || ch == '.';
									 })
									 .take(scheme::max_length)
									 .collect());
			} else {
				throw parse_error("An url must start with an alpha character");
			}
		}

		path_word::path_word(std::string scheme) : _word(std::move(scheme)) {}

		task<path_word> path_word::parse(buffered_istream &stream) {
			co_return path_word(co_await make_adapter(unescape_stream(wrap_adapter(stream)
																		  .take_while([](int ch) {
																			  return is_unreserved(ch) || ch == '%' ||
																					 ch == ':' || ch == '@' ||
																					 ch == '=' || ch == '+' ||
																					 ch == '$' || ch == ',';
																		  })
																		  .into_inner()))
									.take(path_word::max_length)
									.collect());
		}

		segment::segment(path_word path, std::vector<path_word> params)
			: _path(std::move(path)), _params(std::move(params)) {}

		task<segment> segment::parse(buffered_istream &stream) {
			path_word path = co_await path_word::parse(stream);
			std::vector<path_word> params;

			while (co_await stream.peek() == ';') {
				if (params.size() > segment::max_params) {
					throw parse_error("Too many segment parameters");
				}
				co_await stream.get();

				path_word param = co_await path_word::parse(stream);
				if (!param.get().empty()) {
					params.push_back(std::move(param));
				}
			}
			co_return segment(std::move(path), std::move(params));
		}

		path_segments::path_segments(std::vector<segment> segments) : _segments(std::move(segments)) {}

		task<path_segments> path_segments::parse(buffered_istream &stream) {
			std::vector<segment> segments;
			segments.push_back(co_await segment::parse(stream));

			while (co_await stream.peek() == '/') {
				co_await stream.get();
				segments.push_back(co_await segment::parse(stream));
			}
			co_return path_segments(std::move(segments));
		}

		abs_path::abs_path(path_segments segments) : _segments(std::move(segments)) {}

		task<abs_path> abs_path::parse(buffered_istream &stream) {
			co_await expect(stream, '/');
			co_return abs_path(co_await path_segments::parse(stream));
		}
	} // namespace uri
} // namespace cobra
