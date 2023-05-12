#ifndef COBRA_HTTP_HH
#define COBRA_HTTP_HH

#include <stdexcept>

#include <memory>
#include <string>
#include <unordered_map>
#include "cobra/asio.hh"

namespace cobra {

	constexpr std::size_t max_uri_length = 1024;
	constexpr std::size_t max_method_length = 1024;
	constexpr std::size_t max_header_key_length = 1024;
	constexpr std::size_t max_header_value_length = 1024;
	constexpr std::size_t max_header_count = 1024;
	constexpr std::size_t max_reason_phrase_length = 1024;

	enum class http_status_code {
		ok = 200,
		bad_request = 400,
		content_too_large = 413,
		uri_too_long = 414,
		request_header_fields_too_large = 431,
		not_implemented = 501,
	};

	class path {
		std::vector<std::string> segments;
	public:
		void push_back(const std::string& segment);
	};

	class http_version {
	public:
		using version_type = unsigned int;

	private:
		version_type _major = 0;
		version_type _minor = 0;

	public:
		http_version() = default;
		http_version(version_type major, version_type minor);
		http_version(const http_version& other) = default;

		http_version& operator=(const http_version& other) = default;

		inline version_type major() const { return _major; }
		inline version_type minor() const { return _minor; }
	};

	std::string to_status_text(http_status_code status_code);

	class http_error : public std::runtime_error {
		using _base = std::runtime_error;
		http_status_code _status_code;

	public:
		http_error(http_status_code status_code);
		http_error(http_status_code status_code, const std::string& what);
		http_error(http_status_code status_code, const char* what);

		inline http_status_code status_code() const { return _status_code; }
	};

	class header_map {
		using map_type = std::unordered_map<std::string, std::string>;
		map_type map;

	public:
		using key_type = map_type::key_type;
		using mapped_type = map_type::mapped_type;
		using value_type = map_type::value_type;
		using size_type = map_type::size_type;
		using difference_type = map_type::difference_type;
		using hasher = map_type::hasher;
		using key_equal = map_type::key_equal;
		using allocator_type = map_type::allocator_type;
		using reference = map_type::reference;
		using const_reference = map_type::const_reference;
		using pointer = map_type::pointer;
		using const_pointer = map_type::const_pointer;
		using iterator = map_type::iterator;
		using const_iterator = map_type::const_iterator;

		mapped_type& insert_or_assign(const key_type& key, const mapped_type& value);
		mapped_type& insert_or_append(const key_type& key, const mapped_type& value);
		const mapped_type& at(const key_type& key) const;
		bool contains(const key_type& key) const;
		size_type size() const;

		iterator begin();
		const_iterator begin() const;
		iterator end();
		const_iterator end() const;
	};

	enum class http_method  {
		unknown,
		options,
		get,
		head,
		post,
		put,
		del,
		trace,
		connect,
	};

	class http_message {
		header_map _headers;
		http_version _version;

	public:
		http_message(http_version version);
		http_message(http_version version, header_map map);

		inline const header_map& headers() const { return _headers; }
		inline header_map& headers() { return _headers; };
		inline const http_version& version() const { return _version; }
		inline http_version& version() { return _version; }
	};

	class http_request : public http_message {
		std::string _method;
		std::string _request_uri;

	public:
		http_request() = delete;
		http_request(std::string method, std::string uri, http_version version);
		http_request(std::string method, std::string uri, http_version version, header_map map);

		inline const std::string& method() const { return _method; }
		inline std::string& method() { return _method; }
		inline const std::string& request_uri() const { return _request_uri; }
		inline std::string& request_uri() { return _request_uri; }
	};

	class http_response : public http_message {
		unsigned int _status_code;
		std::string _reason_phrase;

	public:
		http_response(http_version version, unsigned int status_code, std::string reason_phrase = std::string());
		http_response(http_version version, unsigned int status_code, std::string reason_phrase, header_map map);

		http_response(http_version version, http_status_code status_code, std::string reason_phrase = std::string());
		http_response(http_version version, http_status_code status_code, std::string reason_phrase, header_map map);

		inline const unsigned int& status_code() const { return _status_code; }
		inline unsigned int& status_code() { return _status_code; }
		inline const std::string& reason_phrase() const { return _reason_phrase; }
		inline std::string& reason_phrase() { return _reason_phrase; }
	};

	http_method parse_method(const std::string& string);
	//future<http_request> parse_request(buffered_istream<& stream);
	future<header_map> parse_headers(istream& stream);
	future<http_request> parse_request(buffered_istream& stream);
	future<http_response> parse_response(buffered_istream& stream); // TODO

	std::ostream& operator<<(std::ostream& os, const header_map& headers);
	std::ostream& operator<<(std::ostream& os, const http_request& request);
	std::ostream& operator<<(std::ostream& os, const http_response& response);

	future<unit> write_request(ostream& stream, const http_request& request);
	future<unit> write_response(ostream& stream, const http_response& response);

	bool is_ctl(int ch);
	bool is_separator(int ch);
	bool is_token(int ch);
	bool is_token(const std::string& str);
	bool is_crlf(int ch);
	future<int> get_ch(istream& stream);
}
#endif
