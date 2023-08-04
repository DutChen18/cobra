#ifndef COBRA_NET_STREAM_HH
#define COBRA_NET_STREAM_HH

#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/net/address.hh"
#include "cobra/config.hh"

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <mutex>
#include <unordered_map>

extern "C" {
#include <openssl/ssl.h>
}


namespace cobra {
	enum class shutdown_how {
		read,
		write,
		both
	};

	class basic_socket_stream : public istream_impl<basic_socket_stream>, public ostream_impl<basic_socket_stream> {
	public:
		virtual ~basic_socket_stream();
		virtual task<std::size_t> read(char_type* data, std::size_t size) = 0;
		virtual task<std::size_t> write(const char_type* data, std::size_t size) = 0;
		virtual task<void> flush() = 0;
		virtual task<void> shutdown(shutdown_how how) = 0;
		virtual address peername() const = 0;
		virtual std::optional<std::string_view> server_name() const = 0;
	};


	class socket_stream : public basic_socket_stream {
		event_loop* _loop;
		file _file;

		friend class ssl_socket_stream;
	public:
		socket_stream(socket_stream&& other);
		socket_stream(event_loop* loop, file&& f);
		~socket_stream();

		task<std::size_t> read(char_type* data, std::size_t size) override;
		task<std::size_t> write(const char_type* data, std::size_t size) override;
		task<void> flush() override;
		task<void> shutdown(shutdown_how how) override;
		address peername() const override;
		std::optional<std::string_view> server_name() const override;
		inline file leak() && { return std::move(_file); }
	};

	class ssl_error : std::runtime_error {
		using error_type = unsigned long;
		std::vector<error_type> _errors;

		ssl_error() = delete;
		ssl_error(const std::string& what, std::vector<error_type> errors);
	public:
		ssl_error(const std::string& what);

	private:
		static std::vector<error_type> get_all_errors();
	};

	class ssl_ctx {
		SSL_CTX *_ctx;

		class tlsext_ctx {
			std::mutex _mtx;
			std::unordered_map<std::string, ssl_ctx> _server_names;

		public:
			tlsext_ctx() = delete;
			tlsext_ctx(std::unordered_map<std::string, ssl_ctx> server_names);
			tlsext_ctx(tlsext_ctx&& other);

			tlsext_ctx& operator=(tlsext_ctx&& other);

			inline auto& mtx() { return _mtx; }
			inline const auto& server_names() const { return _server_names; }
		};

		std::shared_ptr<tlsext_ctx> _ext;

		ssl_ctx(const SSL_METHOD* method);
	public:
		ssl_ctx() = delete;
		ssl_ctx(const ssl_ctx& other);
		~ssl_ctx();

		ssl_ctx& operator=(const ssl_ctx& other);

		static ssl_ctx server(std::unordered_map<std::string, ssl_ctx> server_names);
		static ssl_ctx server(const std::filesystem::path& cert, const std::filesystem::path& key);
		static ssl_ctx client();

		friend class ssl;

	private:
		static ssl_ctx server();
		static int ssl_servername_callback(SSL* s, int*, void* arg);
		static void ref_up(SSL_CTX *ctx);
		static void ref_down(SSL_CTX *ctx);
	};

	class ssl {
		SSL* _ssl;
		ssl_ctx _ctx;//maybe not needed since openssl internally increase the reference count for the context

	public:
		ssl(ssl&& other) noexcept;
		//ssl(const ssl& other);
		ssl(ssl_ctx ctx);
		~ssl();

		inline SSL* ptr() { return _ssl; }
		inline const SSL* ptr() const { return _ssl; }

		void set_file(const file& f);
		task<void> shutdown(event_loop* _loop, file&& f);
		task<void> shutdown(event_loop* _loop, const file& f);
	};

	class ssl_socket_stream : public basic_socket_stream {
		executor* _exec;
		event_loop* _loop;
		file _file;
		ssl _ssl;
		bool _write_shutdown;
		bool _read_shutdown;

		bool _bad;

		ssl_socket_stream() = delete;
		ssl_socket_stream(event_loop* loop, file&& f, ssl&& _ssl);
		ssl_socket_stream(executor* exec, event_loop* loop, file&& f, ssl&& _ssl);
	public:
		ssl_socket_stream(ssl_socket_stream&& other);
		~ssl_socket_stream();

		task<std::size_t> read(char_type* data, std::size_t size) override;
		task<std::size_t> write(const char_type* data, std::size_t size) override;
		task<void> flush() override;
		task<void> shutdown(shutdown_how how) override;
		address peername() const override;
		std::optional<std::string_view> server_name() const override;

		static task<ssl_socket_stream> accept(executor* exec, event_loop* loop, socket_stream&& f, ssl&& ssl);
		static task<ssl_socket_stream> connect(executor* exec, event_loop* loop, socket_stream&& f, ssl&& ssl);
		[[deprecated]] static task<ssl_socket_stream> connect(event_loop* loop, socket_stream&& f, ssl&& ssl);

	private:
		void set_bad();
		inline bool bad() const { return _bad; }
		inline bool can_read() const { return !bad() && !_read_shutdown; }
		inline bool can_write() const { return !bad() && !_write_shutdown; }
		void shutdown_read();
		task<bool> rw_check(int rc);
		task<void> shutdown_write();
	};

	task<socket_stream> open_connection(event_loop* loop, const char* node, const char* service);
	task<ssl_socket_stream> open_ssl_connection(executor* exec, event_loop* loop, const char* node, const char* service);
	task<void> start_server(executor* exec, event_loop* loop, const char* node, const char* service,
							std::function<task<void>(socket_stream)> cb);
	//TODO implement sessions? https://wiki.openssl.org/index.php/SSL_and_TLS_Protocols#Session_Resumption
	task<void> start_ssl_server(ssl_ctx ctx, executor* exec, event_loop* loop, const char* node, const char* service,
							std::function<task<void>(ssl_socket_stream)> cb);
	task<void> start_ssl_server(std::unordered_map<std::string, ssl_ctx> server_names, executor* exec,
								event_loop* loop, const char* node, const char* service,
								std::function<task<void>(ssl_socket_stream)> cb);
} // namespace cobra

#endif
