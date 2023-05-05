#ifndef COBRA_SOCKET_HH
#define COBRA_SOCKET_HH

#include <functional>
#include "cobra/future.hh"

extern "C" {
#include <netdb.h>
}

namespace cobra {

	class addr_info {
		addrinfo *internal = nullptr;

	public:
		class addr_info_iterator {
			addrinfo *ptr;

		public:
			addr_info_iterator() {
				ptr = nullptr;
			}

			addr_info_iterator(addrinfo* ptr) {
				this->ptr = ptr;
			}

			addr_info_iterator(const addr_info_iterator& other) {
				ptr = other.ptr;
			}

			addr_info_iterator& operator++() {
				ptr = ptr->ai_next;
				return *this;
			}

			addr_info_iterator operator++(int) {
				addr_info_iterator old = *this;
				ptr = ptr->ai_next;
				return old;
			}

			bool operator==(const addr_info_iterator &other) const {
				return ptr == other.ptr;
			}

			bool operator!=(const addr_info_iterator &other) const {
				return !(*this == other);
			}

			const addrinfo& operator*() const {
				return *ptr;
			}

			const addrinfo* operator->() const {
				return ptr;
			}
		};

		using value_type = addr_info;
		using iterator = addr_info_iterator;

		addr_info(const std::string& host, const std::string& service);
		~addr_info();

		iterator begin() {
			return addr_info_iterator(internal);
		}

		iterator end() {
			return addr_info_iterator();
		}
	};

	class socket {
		int socket_fd = -1;

	protected:
		socket(int fd);
		socket(const std::string& host, const std::string& service);
		socket(socket&& other) noexcept;
		virtual ~socket();

		socket& operator=(socket&& other) noexcept;
		int get_socket_fd() const;

	private:
		void set_non_blocking(int fd);
	};

	class iosocket : public socket {
		sockaddr addr;
		socklen_t addrlen;

	public:
		iosocket(int fd, sockaddr addr, socklen_t addrlen);
		iosocket(iosocket&& other) noexcept;

		iosocket& operator=(iosocket&& other) noexcept;
		future<std::size_t> read(void* dst, std::size_t count);
		future<std::size_t> write(const void* data, std::size_t count);
	};

	class server : public socket {
		using callback_type = std::function<future<>(iosocket&&)>;
		const callback_type callback;

		int listen_fd;

	public:
		server(const std::string& host, const std::string& service, callback_type callback, int backlog = 5);
		~server();

		future<> start();
	};
}

#endif
