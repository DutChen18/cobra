#include "cobra/http/handler.hh"
#include "cobra/http/parse.hh"
#include "cobra/asyncio/std_stream.hh"
#include "cobra/asyncio/generator_stream.hh"
#include "cobra/net/stream.hh"
#include "cobra/process.hh"
#include "cobra/print.hh"
#include "cobra/fastcgi.hh"
#include "cobra/serde.hh"

#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace cobra {
	// ODOT: sanitize header keys and values
	static generator<std::pair<std::string, std::string>> get_cgi_params(const handle_context<cgi_config>& context) {
		co_yield { "REQUEST_METHOD", context.request().method() };
		co_yield { "SCRIPT_FILENAME", context.root() + context.file() };
		// co_yield { "SCRIPT_NAME", context.file() };
		co_yield { "REDIRECT_STATUS", "200" };

		std::string path_info = context.request().uri().get<uri_origin>()->path().string();

		if (path_info == context.file() || path_info.ends_with("/")) {
			co_yield { "PATH_INFO", path_info };
		} else {
			co_yield { "PATH_INFO", path_info + "/" };
		}

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

	// ODOT: handle all the stuffs
	// ODOT: properly handle parse_http_response errors and stuff
	/*
	static task<std::pair<http_response, std::vector<char>>> get_response(basic_socket_stream& socket, const http_request& request) {
		istream_buffer socket_istream(make_istream_ref(socket), COBRA_BUFFER_SIZE);
		ostream_buffer socket_ostream(make_ostream_ref(socket), COBRA_BUFFER_SIZE);
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

	generator<std::string> list_directories(const std::filesystem::path& path, const std::string& file) {
		co_yield "<!DOCTYPE html>\n<html>\n<body>";
		co_yield std::format("<h1>Index of {}</h1>", file);
		co_yield "<table order=\"\">\n<thead>\n<tr>\n<th>Name</th><th>Size</th><th>Last Modified</th></tr></thead>";
		co_yield "<tbody>";

		if (file != "/") {
			co_yield std::format("<tr><td><a href=\"..\">..</a></td><td></td><td></td></tr>");
		}

		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			std::error_code ec;

			co_yield "<tr>";
			std::string filename = entry.path().filename().string();

			if (entry.is_directory(ec)) {
				co_yield std::format("<td><a href=\"{}/\"i>{}</a></td>", filename, filename);
				co_yield std::format("<td></td>");
			} else {
				if (ec) {
					co_yield "<td>error</td>";
					co_yield "<td>error</td>";
				} else {
				co_yield std::format("<td><a href=\"{}\"i>{}</a></td>", filename, filename);
					co_yield std::format("<td>{}</td>", entry.file_size());
				}

			}

			auto last_modified = entry.last_write_time(ec);

			if (ec) {
				co_yield "<td>error</td>";
			} else {
				co_yield std::format("<td>{}</td>", last_modified);
			}
			co_yield "</tr>";
		}
		co_yield "</table></body></html>";
	}

	task<void> handle_static(http_response_writer writer, const handle_context<static_config>& context,
							 std::optional<http_response_code> code) {
		std::filesystem::path path = context.root() + context.file();

		try {
			if (std::filesystem::is_directory(path)) {
				if (context.config().list_dir()) {
					generator_stream dir_istream(list_directories(path, context.file()));
					http_response resp(code.value_or(HTTP_OK));
					resp.set_header("Content-type", "text/html");
					http_ostream sock_ostream = co_await std::move(writer).send(resp);
					co_await pipe(buffered_istream_reference(dir_istream), ostream_reference(sock_ostream));
					co_return;
				} else {
					throw HTTP_NOT_FOUND;
				}
			}
		} catch (const std::filesystem::filesystem_error&) {
			throw HTTP_NOT_FOUND;
		}

		try {
			std::size_t size = std::filesystem::file_size(path);
			//istream_buffer file_istream(file_istream(path.c_str()), COBRA_BUFFER_SIZE); doesn't work on g++
			istream_buffer fis(file_istream(path.c_str()), COBRA_BUFFER_SIZE);

			if (!fis.inner()) {
				throw HTTP_NOT_FOUND;
			}

			http_response resp(code.value_or(HTTP_OK));

			if (!writer.can_compress()) {
				resp.add_header("Content-Length", std::format("{}", size));
			}

			http_ostream sock_ostream = co_await std::move(writer).send(resp);
			co_await pipe(buffered_istream_reference(fis), ostream_reference(sock_ostream));
		} catch (const std::filesystem::filesystem_error&) {
			throw HTTP_NOT_FOUND;
		} catch (const std::ifstream::failure&) {
			throw HTTP_NOT_FOUND;
		}
	}

	task<void> handle_cgi_response(buffered_istream_reference istream, http_response_writer writer) {
		http_header_map header_map = co_await parse_cgi(istream);
		http_response_code code = 200;
		std::string reason_phrase;

		if (header_map.contains("Status")) {
			auto& val = header_map.at("Status");
			if (val.length() < 5) {
				throw HTTP_BAD_GATEWAY;
			}

			auto tmp = parse_unsigned_strict<http_response_code>(val.substr(0, 3), 999);
			if (!tmp)
				throw HTTP_BAD_GATEWAY;
			code = *tmp;

			//code = std::stoi(val.substr(0, 3));
			if (val[3] != ' ') {
				throw HTTP_BAD_GATEWAY;
			}

			reason_phrase = val.substr(4);
		}

		http_response response(code, std::move(reason_phrase));

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

	template <class T>
	static task<void> try_await(std::exception_ptr& ptr, T& awaitable) {
		try {
			co_await awaitable;
		} catch (...) {
			ptr = std::current_exception();
		}
	}

	static void try_throw(std::exception_ptr ptr) {
		if (ptr) {
			std::rethrow_exception(ptr);
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
			istream_buffer proc_istream(make_istream_ref(proc.out()), COBRA_BUFFER_SIZE);
			ostream_buffer proc_ostream(make_ostream_ref(proc.in()), COBRA_BUFFER_SIZE);

			auto proc_writer = context.exec()->schedule([](auto sock, auto& proc) -> task<void> {
				std::exception_ptr ptr;
				auto coro = pipe(sock, ostream_reference(proc));
				co_await try_await(ptr, coro);
				proc.inner().ptr()->close();
				try_throw(ptr);
			}(context.istream(), proc_ostream));

			auto sock_writer = context.exec()->schedule([](auto& proc, auto writer) -> task<void> {
				co_await handle_cgi_response(proc, std::move(writer));
			}(proc_istream, std::move(writer)));

			std::exception_ptr ptr;
			co_await try_await(ptr, proc_writer);
			co_await try_await(ptr, sock_writer);
			auto coro = proc.wait();
			co_await try_await(ptr, coro);
			try_throw(ptr);
		} else if (const auto* config = context.config().addr()) {
			socket_stream fcgi = co_await open_connection(context.loop(), config->node().c_str(), config->service().c_str());
			istream_buffer fcgi_connection_istream(make_istream_ref(fcgi), COBRA_BUFFER_SIZE);
			ostream_buffer fcgi_connection_ostream(make_ostream_ref(fcgi), COBRA_BUFFER_SIZE);
			fastcgi_client_connection fcgi_connection(fcgi_connection_istream, fcgi_connection_ostream);
			std::shared_ptr<fastcgi_client> fcgi_client = co_await fcgi_connection.begin();
			ostream_buffer fcgi_pstream(make_ostream_ref(fcgi_client->fcgi_params()), COBRA_BUFFER_SIZE);
			istream_buffer fcgi_istream(make_istream_ref(fcgi_client->fcgi_stdout()), COBRA_BUFFER_SIZE);
			ostream_buffer fcgi_ostream(make_ostream_ref(fcgi_client->fcgi_stdin()), COBRA_BUFFER_SIZE);
			istream_buffer fcgi_estream(make_istream_ref(fcgi_client->fcgi_stderr()), COBRA_BUFFER_SIZE);

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
				std::exception_ptr ptr;
				auto coro = pipe(sock, ostream_reference(fcgi));
				co_await try_await(ptr, coro);
				co_await fcgi.inner().ptr()->close();
				try_throw(ptr);
			}(context.istream(), fcgi_ostream));

			auto sock_writer = context.exec()->schedule([](auto& fcgi, auto writer) -> task<void> {
				co_await handle_cgi_response(fcgi, std::move(writer));
			}(fcgi_istream, std::move(writer)));

			while (co_await fcgi_connection.poll());

			// co_await fcgi_logger;
			std::exception_ptr ptr;
			co_await try_await(ptr, fcgi_writer);
			co_await try_await(ptr, sock_writer);
			try_throw(ptr);
		}
	}

	task<void> handle_redirect(http_response_writer writer, const handle_context<redirect_config>& context) {
		std::string path = context.config().root() + context.file();
		http_response response(context.config().code());
		response.set_header("Location", path);
		co_await std::move(writer).send(response);
	}

	void forward_headers(http_message& to, const http_message& from) {
		for (const auto& [key, value] : from.header_map()) {
			if (key.starts_with("Access-Control-") || key.starts_with("Sec-")) {
				to.add_header(key, value);
			}
		}

		to.add_header("Host", from.header_map());
		to.add_header("Cookie", from.header_map());
		to.add_header("Set-Cookie", from.header_map());
		to.add_header("Origin", from.header_map());
		to.add_header("Content-Type", from.header_map());
		to.add_header("Content-Length", from.header_map());
		to.add_header("Accept", from.header_map());
		to.add_header("Location", from.header_map());
		to.add_header("Upgrade", from.header_map());

		if (has_header_value(from, "Connection", "upgrade")) {
			to.add_header("Connection", "Upgrade");
		}
	}

	task<void> handle_proxy(http_response_writer writer, const handle_context<proxy_config>& context) {
		try {
			socket_stream gate = co_await open_connection(context.loop(), context.config().node().c_str(), context.config().service().c_str());
			istream_buffer gate_istream(make_istream_ref(gate), COBRA_BUFFER_SIZE);
			ostream_buffer gate_ostream(make_ostream_ref(gate), COBRA_BUFFER_SIZE);
			http_request gate_request(context.request().method(), context.request().uri());
			forward_headers(gate_request, context.request());

			co_await write_http_request(gate_ostream, gate_request);

			auto gate_writer = context.exec()->schedule([](auto sock, auto& gate) -> task<void> {
				co_await pipe(sock, ostream_reference(gate));
				// co_await gate.inner().ptr()->shutdown(shutdown_how::write);
			}(context.istream(), gate_ostream));

			auto sock_writer = context.exec()->schedule([](auto& gate, auto writer) -> task<void> {
				http_response gate_response = co_await parse_http_response(gate);
				http_response response(gate_response.code(), gate_response.reason());
				forward_headers(response, gate_response);
				http_ostream sock = co_await std::move(writer).send(response);
				http_istream_variant<buffered_istream_reference> gate_stream = get_istream(buffered_istream_reference(gate), gate_response);
				co_await pipe(buffered_istream_reference(gate_stream), ostream_reference(sock));
			}(gate_istream, std::move(writer)));

			co_await gate_writer;
			co_await sock_writer;
		} catch (const connection_error& ex) {
			eprintln("connection error: {}", ex.what());
			throw HTTP_BAD_GATEWAY;
		} catch (http_parse_error err) {
			eprintln("parse_error {}", static_cast<int>(err));
			throw HTTP_BAD_GATEWAY;
		} catch (stream_error) {
			eprintln("stream_error");
			throw HTTP_BAD_GATEWAY;
		} catch (compress_error) {
			eprintln("compress_error");
			throw HTTP_BAD_GATEWAY;
		} 
	}
}
