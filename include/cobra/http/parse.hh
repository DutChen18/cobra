#ifndef COBRA_HTTP_PARSE_HH
#define COBRA_HTTP_PARSE_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/http/message.hh"
#include "cobra/http/uri.hh"
#include <limits>
#include <optional>
#include <charconv>

namespace cobra {
	constexpr std::size_t http_header_key_max_length = 256;
	constexpr std::size_t http_header_value_max_length = 4096;
	constexpr std::size_t http_header_map_max_length = 256;
	constexpr std::size_t http_header_map_max_size = 65536;
	constexpr std::size_t http_request_method_max_length = 256;
	constexpr std::size_t http_request_uri_max_length = 4096;
	constexpr std::size_t http_response_reason_max_length = 256;
	constexpr std::size_t cgi_header_key_max_length = 256;
	constexpr std::size_t cgi_header_value_max_length = 4096;
	constexpr std::size_t cgi_header_map_max_length = 256;
	constexpr std::size_t cgi_header_map_max_size = 65536;

	enum class http_parse_error {
		unexpected_eof,
		bad_eol,
		bad_header,
		bad_header_key,
		bad_header_value,
		header_key_too_long,
		header_value_too_long,
		header_map_too_long,
		header_map_too_large,
		header_map_duplicate,
		bad_request_method,
		bad_request_uri,
		bad_version,
		bad_response_code,
		bad_response_reason,
		empty_header_key,
		empty_request_method,
		request_method_too_long,
		request_uri_too_long,
		response_reason_too_long,
		bad_content,
	};

	enum class uri_parse_error {
		bad_uri,
		bad_escape,
		bad_segment,
		bad_query,
		bad_asterisk,
	};

	uri_origin parse_uri_origin(std::string_view string);
	uri_absolute parse_uri_absolute(std::string_view string);
	uri_authority parse_uri_authority(std::string_view string);
	uri_asterisk parse_uri_asterisk(std::string_view string);
	uri parse_uri(std::string_view string, const http_request_method& method);
	task<http_request> parse_http_request(buffered_istream_reference stream);
	task<http_response> parse_http_response(buffered_istream_reference stream);
	task<http_header_map> parse_cgi(buffered_istream_reference stream);

	template <class UnsignedT>
	std::optional<UnsignedT> parse_unsigned_strict(std::string_view str,
											const UnsignedT max_value = std::numeric_limits<UnsignedT>::max()) {
		UnsignedT value;

		const std::from_chars_result result = std::from_chars(str.data(), str.data() + str.length(), value);

		if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range ||
			value > max_value)
			return std::nullopt;

		if (result.ptr != str.data() + str.length())
			return std::nullopt;
		return value;
	}
}

#endif
