#ifndef COBRA_HTTP_HH
#define COBRA_HTTP_HH

#include "cobra/asyncio/stream.hh"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace cobra {

	class http_exception : public std::runtime_error {};

	class http_token {
		std::string _token;

		explicit http_token(std::string token);

	public:
		static constexpr std::size_t max_length = 1024;

		http_token() = delete;
		http_token(const http_token& other) = default;
		http_token(http_token&& other) noexcept = default;

		http_token& operator=(const http_token& other) = default;
		http_token& operator=(http_token&& other) noexcept = default;

		inline const std::string& get() const {
			return _token;
		}
		friend task<http_token> parse(buffered_istream& stream);
		friend task<http_token> parse(istream& stream);
		friend http_token parse(const std::string& str);
	};

	struct http_version {
		using version_type = unsigned int;
		version_type major;
		version_type minor;
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

	class http_message {
		http_version _version;
		header_map _headers;

	public:
		http_message() = delete;
		http_message(http_version version, header_map map);
		http_message(const http_message& other) = default;
		http_message(http_message&& other) noexcept = default;

		inline const header_map& headers() const {
			return _headers;
		}
		inline header_map& headers() {
			return _headers;
		}
		inline const http_version& version() const {
			return _version;
		}
		inline http_version& version() {
			return _version;
		}
	};

	class http_method {
		http_token _method;

	public:
		http_method() = delete;
		http_method(http_token token);

		inline const std::string& method() const {
			return token().get();
		}
		inline const http_token& token() const {
			return _method;
		}
		inline http_token token() {
			return _method;
		}
	};

	class http_request_uri {
		std::string _request_uri;

		http_request_uri(std::string request_uri);

	public:
		http_request_uri() = delete;
		http_request_uri(const http_request_uri& other) = default;
		http_request_uri(http_request_uri&& other) noexcept = default;

		http_request_uri& operator=(const http_request_uri& other) = default;
		http_request_uri& operator=(http_request_uri&& other) noexcept = default;
	};

	class http_request : public http_message {
		http_method _method;
		http_request_uri _request_uri;
	};

	bool is_separator(int ch);
	bool is_ctl(int ch);
} // namespace cobra

#endif
