#ifndef COBRA_HTTP_PARSE_HH
#define COBRA_HTTP_PARSE_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/http/message.hh"

namespace cobra {
	constexpr std::size_t http_header_key_max_length = 256;
	constexpr std::size_t http_header_value_max_length = 4096;
	constexpr std::size_t http_header_map_max_length = 256;
	constexpr std::size_t http_header_map_max_size = 65536;
	constexpr std::size_t http_request_method_max_length = 256;
	constexpr std::size_t http_request_uri_max_length = 4096;
	constexpr std::size_t http_response_reason_max_length = 256;

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
	};

	task<http_request> parse_http_request(buffered_istream_reference stream);
	task<http_response> parse_http_response(buffered_istream_reference stream);
}

#endif
