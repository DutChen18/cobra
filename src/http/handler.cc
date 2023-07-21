#include "cobra/http/handler.hh"
#include "cobra/http/parse.hh"
#include "cobra/asyncio/std_stream.hh"
#include "cobra/net/stream.hh"
#include "cobra/process.hh"
#include "cobra/print.hh"
#include "cobra/fastcgi.hh"
#include "cobra/serde.hh"

#include <fstream>

namespace cobra {
	// TODO: sanitize header keys and values
	static generator<std::pair<std::string, std::string>> get_cgi_params(const handle_context<cgi_config>& context) {
		co_yield { "REQUEST_METHOD", context.request().method() };
		co_yield { "SCRIPT_FILENAME", context.config().root() + context.file() };
		co_yield { "REDIRECT_STATUS", "200" };

		if (context.request().has_header("Content-Length")) {
			co_yield { "CONTENT_LENGTH", context.request().header("Content-Length") };
		}

		if (context.request().has_header("Content-Type")) {
			co_yield { "CONTENT_TYPE", context.request().header("Content-Type") };
		}

		for (const auto& [http_key, http_value] : context.request().header_map()) {
			std::string key = "HTTP_";

			for (char ch : http_key) {
				key.push_back(ch == '-' ? '_' : std::toupper(ch));
			}

			co_yield { key, http_value };
}
	}

	static_config::static_config(std::string root) : _root(std::move(root)) {
	}

	const std::string& static_config::root() const {
		return _root;
	}

	cgi_command::cgi_command(std::string cmd) : _cmd(std::move(cmd)) {
	}

	const std::string& cgi_command::cmd() const {
		return _cmd;
	}

	cgi_address::cgi_address(std::string node, std::string service) : _node(std::move(node)), _service(std::move(service)) {
	}

	const std::string& cgi_address::node() const {
		return _node;
	}

	const std::string& cgi_address::service() const {
		return _service;
	}

	cgi_config::cgi_config(std::string root, cgi_command cmd) : _root(std::move(root)), _config(std::move(cmd)) {
	}

	cgi_config::cgi_config(std::string root, cgi_address addr) : _root(std::move(root)), _config(std::move(addr)) {
	}

	const std::string& cgi_config::root() const {
		return _root;
	}

	const cgi_command* cgi_config::cmd() const {
		return std::get_if<cgi_command>(&_config);
	}

	const cgi_address* cgi_config::addr() const {
		return std::get_if<cgi_address>(&_config);
	}

	// TODO: 404 not found
	// TODO: move Connection: close to http_response_writer
	task<void> handle_static(http_response_writer writer, const handle_context<static_config>& context) {
		std::string path = context.config().root() + context.file();
		istream_buffer file_istream(std_istream(std::ifstream(path, std::ifstream::binary)), 1024);
		http_response response(HTTP_OK);
		response.set_header("Connection", "close");
		http_ostream sock_ostream = co_await std::move(writer).send(response);
		co_await pipe(buffered_istream_reference(file_istream), ostream_reference(sock_ostream));
	}

	// TODO: response headers from cgi
	// TODO: move Connection: close to http_response_writer
	task<void> handle_cgi(http_response_writer writer, const handle_context<cgi_config>& context) {
		if (const auto* config = context.config().cmd()) {
			command cmd({ config->cmd(), context.config().root() + context.file() });

			cmd.in(command_stream_mode::pipe);
			cmd.out(command_stream_mode::pipe);

			for (auto [key, value] : get_cgi_params(context)) {
				cmd.env(std::move(key), std::move(value));
			}

			process proc = cmd.spawn(context.loop());
			istream_buffer proc_istream(make_istream_ref(proc.out()), 1024);
			ostream_buffer proc_ostream(make_ostream_ref(proc.in()), 1024);

			auto proc_writer = context.exec()->schedule([](auto sock, auto& proc) -> task<void> {
				co_await pipe(sock, ostream_reference(proc));
				proc.inner().ptr()->close();
			}(context.istream(), proc_ostream));

			auto sock_writer = context.exec()->schedule([](auto& proc, auto writer) -> task<void> {
				co_await parse_cgi(proc);
				http_response response(HTTP_OK);
				response.set_header("Connection", "close");
				http_ostream sock = co_await std::move(writer).send(response);
				co_await pipe(buffered_istream_reference(proc), ostream_reference(sock));
			}(proc_istream, std::move(writer)));

			co_await proc_writer;
			co_await sock_writer;
			co_await proc.wait();
		} else if (const auto* config = context.config().addr()) {
			socket_stream fcgi = co_await open_connection(context.loop(), config->node().c_str(), config->service().c_str());
			istream_buffer fcgi_connection_istream(make_istream_ref(fcgi), 1024);
			ostream_buffer fcgi_connection_ostream(make_ostream_ref(fcgi), 1024);
			fastcgi_client_connection fcgi_connection(fcgi_connection_istream, fcgi_connection_ostream);
			std::shared_ptr<fastcgi_client> fcgi_client = co_await fcgi_connection.begin();
			ostream_buffer fcgi_pstream(make_ostream_ref(fcgi_client->fcgi_params()), 1024);
			istream_buffer fcgi_istream(make_istream_ref(fcgi_client->fcgi_stdout()), 1024);
			ostream_buffer fcgi_ostream(make_ostream_ref(fcgi_client->fcgi_stdin()), 1024);

			for (auto [key, value] : get_cgi_params(context)) {
				co_await write_u32_be(fcgi_pstream, key.size() | 0x80000000);
				co_await write_u32_be(fcgi_pstream, value.size() | 0x80000000);
				co_await fcgi_pstream.write_all(key.data(), key.size());
				co_await fcgi_pstream.write_all(value.data(), value.size());
			}

			co_await fcgi_pstream.flush();
			co_await fcgi_pstream.inner().ptr()->close();

			auto fcgi_writer = context.exec()->schedule([](auto sock, auto& fcgi) -> task<void> {
				co_await pipe(sock, ostream_reference(fcgi));
				co_await fcgi.inner().ptr()->close();
			}(context.istream(), fcgi_ostream));

			auto sock_writer = context.exec()->schedule([](auto& proc, auto writer) -> task<void> {
				co_await parse_cgi(proc);
				http_response response(HTTP_OK);
				response.set_header("Connection", "close");
				http_ostream sock = co_await std::move(writer).send(response);
				co_await pipe(buffered_istream_reference(proc), ostream_reference(sock));
			}(fcgi_istream, std::move(writer)));

			while (co_await fcgi_connection.poll());

			co_await fcgi_writer;
			co_await sock_writer;
		}
	}
}
