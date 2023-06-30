#include "cobra/uri.hh"

#include "cobra/asyncio/stream_utils.hh"
#include "cobra/parse_utils.hh"

#include <cctype>
#include <optional>
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

		uri_scheme::uri_scheme(std::string scheme) : _uri_scheme(std::move(scheme)) {}

		task<uri_scheme> uri_scheme::parse(buffered_istream &stream) {
			if (is_alpha(co_await assert_get(stream))) {
				co_return uri_scheme(co_await wrap_adapter(stream)
										 .take_while([](int ch) {
											 return is_alpha(ch) || is_digit(ch) || ch == '+' || ch == '-' || ch == '.';
										 })
										 .take(uri_scheme::max_length)
										 .collect());
			} else {
				throw parse_error("An url must start with an alpha character");
			}
		}

		path_word::path_word(std::string uri_scheme) : _word(std::move(uri_scheme)) {}

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

				params.push_back(co_await path_word::parse(stream));
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

		authority::authority(std::string authority) : _authority(std::move(authority)) {}

		task<authority> authority::parse(buffered_istream &stream) {
			co_return authority(co_await make_adapter(stream)
									.take_while([](int ch) { // TODO check if the escaped characters are correct
										return is_unreserved(ch) || ch == '%' || ch == ';' || ch == ':' || ch == '&' ||
											   ch == '=' || ch == '+' || ch == '$' || ch == ',' || ch == '@';
									})
									.take(authority::max_length)
									.collect());
		}

		net_path::net_path(authority auth, std::optional<abs_path> path)
			: _authority(std::move(auth)), _path(std::move(path)) {}

		task<net_path> net_path::parse(buffered_istream &stream) {
			co_await expect(stream, "//");

			authority auth = co_await authority::parse(stream);

			std::optional<abs_path> path;
			if (co_await stream.peek() == '/') {
				path = co_await abs_path::parse(stream);
			}
			co_return net_path(std::move(auth), std::move(path));
		}

		uri_query::uri_query(std::string str) : _uri_query(std::move(str)) {}

		task<uri_query> uri_query::parse(buffered_istream &stream){
			// TODO check if the escaped characters are correct
			co_return uri_query(co_await wrap_adapter(stream)
									.take_while([](int ch) {
										return is_unreserved(ch) || is_reserved(ch) || ch == '%';
									})
									.take(uri_query::max_length)
									.collect()) :
		}

		hier_part::hier_part(std::variant<net_path, abs_path> path, std::optional<uri_query> query)
			: _path(std::move(path)), _query(std::move(query)) {}

		task<hier_part> hier_part::parse(buffered_istream &stream) {
			co_await expect(stream, '/');

			auto ch = co_await expect(stream);
			auto path = ch == '/' ? std::variant<net_path, abs_path>(co_await net_path::parse(stream))
								  : co_await abs_path::parse(stream);
			std::optional<uri_query> query;

			if (co_await stream.peek() == '?') {
				co_await stream.get();
				query = co_await uri_query::parse(stream);
			}
			co_return hier_part(std::move(path), std::move(query));
		}

		opaque_part::opaque_part(std::string opaque) : _opaque(std::move(opaque)) {}

		task<opaque_part> parse(buffered_istream &stream) {
			auto ch = co_await expect_peek(stream);

			// TODO check if escaped characters are correct
			auto is_uric_no_slash = [](int ch) {
				return is_unreserved(ch) || ch == '%' || ch == ';' || ch == '?' || ch == ':' || ch == '@' ||
					   ch == '&' || ch == '=' || ch == '+' || ch == '$' || ch == ',';
			};

			if (is_uric_no_slash(ch)) {
				co_return opaque_part(wrap_adapter(stream)
										  .take_while([](int ch) {
											  return is_uric_no_slash(ch) || ch == '/';
										  })
										  .take(opaque_part::max_length)
										  .collect());
			} else {
				throw parse_error("not uric_no_slash");
			}
		}

		task<abs_uri> abs_uri::parse(buffered_istream &stream) {
			uri_scheme scheme = co_await scheme::parse(stream);
			co_await expect(stream, ':');
			std::variant<hier_part, opaque_part> part = co_await expect(stream) == '/'
															? co_await hier_part::parse(stream)
															: co_await opaque_part::parse(stream);
			co_return abs_uri(std::move(scheme), part);
		}
	} // namespace uri
} // namespace cobra
