#ifndef COBRA_SOCKET_HH
#define COBRA_SOCKET_HH

#include <cstddef>
#include <iterator>
#include <vector>

#include "cobra/future.hh"
#include "cobra/asio.hh"

extern "C" {
#include <netdb.h>
}

namespace cobra {
	class free_delete {
	public:
		void operator()(void* ptr) const;
	};

	class address {
	private:
		std::unique_ptr<sockaddr, free_delete> addr;
		std::size_t len;
	public:
		address(const sockaddr* addr, std::size_t len);
		address(const address& other);
		address(address&& other);

		address& operator=(address other);

		sockaddr* get_addr() const;
		std::size_t get_len() const;
	};

	class address_info {
	private:
		int family;
		int type;
		int proto;
		address addr;
	public:
		address_info(const addrinfo* info);

		int get_family() const;
		int get_type() const;
		int get_proto() const;
		const address& get_addr() const;
	};

	class socket {
	protected:
		int fd;
	public:
		socket(int fd);
		socket(socket&& other);
		socket(const socket&) = delete;
		virtual ~socket();

		socket& operator=(socket other);

		int leak_fd();
	};

	class connected_socket : public socket, public basic_iostream<char> {
	public:
		using char_type = typename basic_iostream<char>::char_type;
		using traits_type = typename basic_iostream<char>::traits_type;
		using int_type = typename traits_type::int_type;
		using pos_type = typename traits_type::pos_type;
		using off_type = typename traits_type::off_type;

		connected_socket(int fd);
		connected_socket(connected_socket&& other);

		connected_socket& operator=(connected_socket other);

		future<std::size_t> read(char_type* dst, std::size_t count) override;
		future<std::size_t> write(const char_type* data, std::size_t count) override;
		future<unit> flush() override; 
	};

	class server_socket : public socket {
	public:
		server_socket(int fd);
		server_socket(server_socket&& other);

		server_socket& operator=(server_socket other);

		future<connected_socket> accept();
	};

	class initial_socket : public socket {
	public:
		initial_socket(int family, int type, int proto);
		initial_socket(initial_socket&& other);

		initial_socket& operator=(initial_socket other);

		void bind(const address& addr);
		future<connected_socket> connect(const address& addr)&&;
		server_socket listen(int backlog = 5)&&;
	};

	std::vector<address_info> get_address_info(const char* host, const char* port, int family = AF_UNSPEC, int type = SOCK_STREAM);
	future<connected_socket> open_connection(const char* host, const char* port);
	future<unit> start_server(const char* host, const char* port, function<future<unit>, connected_socket>&& callback);
}

#endif
