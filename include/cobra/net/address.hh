#ifndef COBRA_NET_ADDRESS_HH
#define COBRA_NET_ADDRESS_HH

#include "cobra/asyncio/generator.hh"

#include <memory>

extern "C" {
#include <netdb.h>
}

#ifdef COBRA_FUZZ_HANDLER
#include <cassert>
#endif

namespace cobra {

#ifdef COBRA_FUZZ_HANDLER
	class address {
		sockaddr* _addr = nullptr;
                std::string _name;
	public:
		address(std::string name) : _name(std::move(name)) {}
		address(const sockaddr*, std::size_t) {
                        assert(0);
                }
		//address(const address& other) = default;
		//address(address&& other) = default;
		//~address() {}

		//address& operator=(address other) = default;

		inline const sockaddr* addr() const {
                        assert(0);
                        return _addr;
		}

		inline std::size_t len() const {
                        assert(0);
                        return 0;
		}

		inline std::string string() const {
                        return _name;
                }
	};
#else
	class address {
		sockaddr* _addr;
		std::size_t _len;

	public:
		address(const sockaddr* addr, std::size_t len);
		address(const address& other);
		address(address&& other);
		~address();

		address& operator=(address other);

		inline const sockaddr* addr() const {
			return _addr;
		}

		inline std::size_t len() const {
			return _len;
		}

		std::string string() const;
	};
#endif

	class address_info {
		int _family;
		int _socktype;
		int _protocol;
		address _addr;

	public:
		address_info(const addrinfo* info);

		inline int family() const {
			return _family;
		}

		inline int socktype() const {
			return _socktype;
		}

		inline int protocol() const {
			return _protocol;
		}

		inline const address& addr() const {
			return _addr;
		}
	};

	generator<address_info> get_address_info(const char* node, const char* service, int family = AF_UNSPEC,
											 int socktype = SOCK_STREAM, int protocol = 0);
}; // namespace cobra

#endif
