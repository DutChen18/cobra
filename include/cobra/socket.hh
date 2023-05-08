#ifndef COBRA_SOCKET_HH
#define COBRA_SOCKET_HH

#include <cstddef>
#include <iterator>

#include "cobra/future.hh"

extern "C" {
#include <netdb.h>
}

namespace cobra {

	class istream_base {
	public:
		virtual future<char> get() = 0;
		virtual char get_unsafe() = 0;
		virtual future<char> peek() = 0;
		virtual future<std::size_t> read_some(void* dst, std::size_t count) = 0;
	};

	class ostream_base {
	public:
		virtual future<std::size_t> write(const void* data, std::size_t count) = 0;
	};

	class addr_info {
		addrinfo *internal = nullptr;

	public:
		class addr_info_iterator {
			addrinfo *ptr;

		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = addrinfo;
			using difference_type = std::ptrdiff_t;
			using pointer = addrinfo*;
			using refrence = addrinfo&;

			addr_info_iterator();
			addr_info_iterator(addrinfo* ptr);
			addr_info_iterator(const addr_info_iterator& other);

			addr_info_iterator& operator++();
			addr_info_iterator operator++(int);

			bool operator==(const addr_info_iterator &other) const;
			bool operator!=(const addr_info_iterator &other) const;

			const addrinfo& operator*() const;
			const addrinfo* operator->() const;
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

		sockaddr_storage addr;
		socklen_t addrlen;

	public:
		socket(socket&& other) noexcept;
		socket& operator=(socket&& other) noexcept;

	protected:
		socket(int fd, sockaddr_storage addr, socklen_t addrlen);
		socket(const std::string& host, const std::string& service);
		virtual ~socket();

		int get_socket_fd() const;

	private:

		void set_non_blocking(int fd);
	};

	class iosocket : public socket {
	public:
		iosocket(int fd, sockaddr_storage addr, socklen_t addrlen);
		iosocket(iosocket&& other) noexcept;

		iosocket& operator=(iosocket&& other) noexcept;
		future<std::size_t> read_some(void* dst, std::size_t count);
		future<std::size_t> write(const void* data, std::size_t count);
	};

	class server : public socket {
		using callback_type = function<future<>, iosocket&&>;
		const callback_type callback;

		int listen_fd = -1;

	public:
		server(const std::string& host, const std::string& service, callback_type&& callback, int backlog = 5);
		~server();

		future<> start();
	};
}

#endif
