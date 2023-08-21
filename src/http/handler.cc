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
		co_yield { "SCRIPT_FILENAME", context.root() + context.file() };
		// co_yield { "SCRIPT_NAME", context.file() };
		co_yield { "PATH_INFO", context.request().uri().get<uri_origin>()->path().string() };
		co_yield { "REDIRECT_STATUS", "200" };

		if (auto query = context.request().uri().get<uri_origin>()->query()) {
			co_yield { "QUERY_STRING", *query };
		}

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

	// TODO: handle all the stuffs
	// TODO: properly handle parse_http_response errors and stuff
	/*
	static task<std::pair<http_response, std::vector<char>>> get_response(basic_socket_stream& socket, const http_request& request) {
		istream_buffer socket_istream(make_istream_ref(socket), 1024);
		ostream_buffer socket_ostream(make_ostream_ref(socket), 1024);
		co_await write_http_request(socket_ostream, request);
		co_await socket.shutdown(shutdown_how::write);
		http_response response = co_await parse_http_response(socket_istream);
		std::vector<char> data;

		while (true) {
			auto [buffer, size] = co_await socket_istream.fill_buf();
			data.insert(data.end(), buffer, buffer + size);

			if (size == 0) {
				break;
			}

			socket_istream.consume(size);
		}

		co_return { response, data };
	}

	static task<std::pair<http_response, std::vector<char>>> send_http_request(event_loop* loop, const http_request& request, std::string node, std::string service = "80") {
		socket_stream socket = co_await open_connection(loop, node.c_str(), service.c_str());
		co_return co_await get_response(socket, request);
	}

	static task<std::pair<http_response, std::vector<char>>> send_https_request(executor* exec, event_loop* loop, const http_request& request, std::string node, std::string service = "443") {
		ssl_socket_stream socket = co_await open_ssl_connection(exec, loop, node.c_str(), service.c_str());
		co_return co_await get_response(socket, request);
	}
	*/

	// TODO: directories don't immediately error
	task<void> handle_static(http_response_writer writer, const handle_context<static_config>& context) {
		try {
			istream_buffer file_istream(std_istream(std::ifstream(context.root() + context.file(), std::ifstream::binary)), 1024);
			co_await file_istream.fill_buf();
			http_response resp(context.config().code().value_or(HTTP_OK));
			// resp.set_header("content-encoding", "br");
			http_ostream sock_ostream = co_await std::move(writer).send(resp);
			// brotli_ostream brotli(std::move(sock_ostream));
			co_await pipe(buffered_istream_reference(file_istream), ostream_reference(sock_ostream));
		} catch (const std::ifstream::failure&) {
			throw HTTP_NOT_FOUND;
		}
	}

	task<void> handle_cgi_response(buffered_istream_reference istream, http_response_writer writer) {
		http_header_map header_map = co_await parse_cgi(istream);
		http_response_code code = 200;

		if (header_map.contains("Status")) {
			// TODO: use reason phrase from status
			code = std::stoi(header_map.at("Status").substr(0, 3));
		}

		http_response response(code);

		response.set_header("Location", header_map);
		response.set_header("Content-Type", header_map);
		response.add_header("Set-Cookie", header_map);

		if (code != 404) {
			http_ostream sock = co_await std::move(writer).send(response);
			co_await pipe(istream, ostream_reference(sock));
		} else {
			throw HTTP_NOT_FOUND;
		}
	}

	task<void> handle_cgi(http_response_writer writer, const handle_context<cgi_config>& context) {
		if (const auto* config = context.config().cmd()) {
			command cmd({ config->cmd(), context.root() + context.file() });

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
				co_await handle_cgi_response(proc, std::move(writer));
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
			istream_buffer fcgi_estream(make_istream_ref(fcgi_client->fcgi_stderr()), 1024);

			for (auto [key, value] : get_cgi_params(context)) {
				co_await write_u32_be(fcgi_pstream, key.size() | 0x80000000);
				co_await write_u32_be(fcgi_pstream, value.size() | 0x80000000);
				co_await fcgi_pstream.write_all(key.data(), key.size());
				co_await fcgi_pstream.write_all(value.data(), value.size());

				// php requires headers not be split across tcp blocks, this is non-standard
				co_await fcgi_pstream.flush();
			}
			
			co_await fcgi_pstream.inner().ptr()->close();
		
			/*
			auto fcgi_logger = context.exec()->schedule([](auto& fcgi) -> task<void> {
				std_ostream_reference std_err(std::cerr);
				co_await pipe(buffered_istream_reference(fcgi), ostream_reference(std_err));
			}(fcgi_estream));
			*/

			auto fcgi_writer = context.exec()->schedule([](auto sock, auto& fcgi) -> task<void> {
				co_await pipe(sock, ostream_reference(fcgi));
				co_await fcgi.inner().ptr()->close();
			}(context.istream(), fcgi_ostream));

			auto sock_writer = context.exec()->schedule([](auto& fcgi, auto writer) -> task<void> {
				co_await handle_cgi_response(fcgi, std::move(writer));
			}(fcgi_istream, std::move(writer)));

			while (co_await fcgi_connection.poll());

			// co_await fcgi_logger;
			co_await fcgi_writer;
			co_await sock_writer;
		}
	}

	task<void> handle_redirect(http_response_writer writer, const handle_context<redirect_config>& context) {
		std::string path = context.config().root() + context.file();
		http_response response(context.config().code());
		response.set_header("Location", path);
		co_await std::move(writer).send(response);
	}

	task<void> handle_proxy(http_response_writer writer, const handle_context<proxy_config>& context) {
		socket_stream gate = co_await open_connection(context.loop(), context.config().node().c_str(), context.config().service().c_str());
		istream_buffer gate_istream(make_istream_ref(gate), 1024);
		ostream_buffer gate_ostream(make_ostream_ref(gate), 1024);
		http_request gate_request(context.request().method(), context.request().uri());

		co_await write_http_request(gate_ostream, gate_request);

		auto gate_writer = context.exec()->schedule([](auto sock, auto& gate) -> task<void> {
			co_await pipe(sock, ostream_reference(gate));
			co_await gate.inner().ptr()->shutdown(shutdown_how::write);
		}(context.istream(), gate_ostream));

		auto sock_writer = context.exec()->schedule([](auto& gate, auto writer) -> task<void> {
			http_response gate_response = co_await parse_http_response(gate);
			http_response response(gate_response.code(), gate_response.reason());
			http_ostream sock = co_await std::move(writer).send(response);
			co_await pipe(buffered_istream_reference(gate), ostream_reference(sock));
		}(gate_istream, std::move(writer)));

		co_await gate_writer;
		co_await sock_writer;
	}
}
