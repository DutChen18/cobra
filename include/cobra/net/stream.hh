#ifndef COBRA_NET_STREAM_HH
#define COBRA_NET_STREAM_HH

#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/net/address.hh"

#include <filesystem>
#include <functional>

extern "C" {
#include <openssl/ssl.h>
}


namespace cobra {
	class socket_stream : public istream_impl<socket_stream>, public ostream_impl<socket_stream> {
		event_loop* _loop;
		file _file;

	public:
		socket_stream(event_loop* loop, file&& f);

		task<std::size_t> read(char_type* data, std::size_t size);
		task<std::size_t> write(const char_type* data, std::size_t size);
		task<void> flush();
		void shutdown(int how);
		address peername() const;
	};

	class ssl_ctx {
		SSL_CTX *_ctx;

	public:
		ssl_ctx() = delete;
		ssl_ctx(const std::filesystem::path& cert, const std::filesystem::path& key);
		ssl_ctx(const ssl_ctx& other);
		~ssl_ctx();

		//inline SSL_CTX *ctx() { return _ctx; }
		SSL* new_ssl();
		inline const SSL_CTX *ctx() const { return _ctx; }
	};

	class ssl_socket_stream : public istream_impl<socket_stream>, public ostream_impl<socket_stream> {
		event_loop* _loop;
		file _file;
		ssl_ctx _ctx;
		SSL* _ssl;

		ssl_socket_stream() = delete;
		ssl_socket_stream(event_loop* loop, file&& f);
	public:
		~ssl_socket_stream();

		task<std::size_t> read(char_type* data, std::size_t size);
		task<std::size_t> write(const char_type* data, std::size_t size);
		task<void> flush();
		void shutdown(int how);
		address peername() const;
	};

	task<socket_stream> open_connection(event_loop* loop, const char* node, const char* service);
	task<ssl_socket_stream> open_connection(ssl_ctx ctx, event_loop* loop, const char* node, const char* service);
	task<void> start_server(executor* exec, event_loop* loop, const char* node, const char* service,
							std::function<task<void>(socket_stream)> cb);
	task<void> start_server(ssl_ctx ctx, executor* exec, event_loop* loop, const char* node, const char* service,
							std::function<task<void>(ssl_socket_stream)> cb);
} // namespace cobra

#endif
