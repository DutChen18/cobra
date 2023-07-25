#include "cobra/net/address.hh"
#include "cobra/file.hh"

#include <cstring>
#include <utility>
#include <format>

namespace cobra {
	address::address(const sockaddr* addr, std::size_t len) : _len(len) {
		_addr = static_cast<sockaddr*>(std::malloc(len));
		std::memcpy(_addr, addr, len);
	}

	address::address(const address& other) : _len(other._len) {
		_addr = static_cast<sockaddr*>(std::malloc(other._len));
		std::memcpy(_addr, other._addr, other._len);
	}

	address::address(address&& other) : _len(other._len) {
		_addr = std::exchange(other._addr, nullptr);
	}

	address::~address() {
		std::free(_addr);
	}

	address& address::operator=(address other) {
		std::swap(_addr, other._addr);
		std::swap(_len, other._len);
		return *this;
	}

	std::string address::string() const {
		char host[1024];
		char service[1024];
		check_return(getnameinfo(_addr, _len, host, sizeof host, service, sizeof service, NI_NUMERICHOST | NI_NUMERICSERV));
		return std::format("{}:{}", host, service);
	}

	address_info::address_info(const addrinfo* info)
		: _family(info->ai_family), _socktype(info->ai_socktype), _protocol(info->ai_protocol),
		  _addr(info->ai_addr, info->ai_addrlen) {}

	generator<address_info> get_address_info(const char* node, const char* service, int family, int socktype,
											 int protocol) {
		addrinfo hints;
		addrinfo* info;

		hints.ai_flags = 0;
		hints.ai_family = family;
		hints.ai_socktype = socktype;
		hints.ai_protocol = protocol;
		hints.ai_addrlen = 0;
		hints.ai_addr = nullptr;
		hints.ai_canonname = nullptr;
		hints.ai_next = nullptr;

		if (node == nullptr) {
			hints.ai_flags |= AI_PASSIVE;
		}

		int ret = getaddrinfo(node, service, &hints, &info);

		if (ret != 0) {
			throw std::runtime_error(gai_strerror(ret));
		}

		for (addrinfo* i = info; i != nullptr; i = i->ai_next) {
			co_yield i;
		}

		freeaddrinfo(info);
	}
} // namespace cobra
