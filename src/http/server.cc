#include "cobra/http/server.hh"

#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/config.hh"
#include "cobra/http/handler.hh"
#include "cobra/http/parse.hh"
#include "cobra/http/gluttonous_stream.hh"
#include "cobra/counter.hh"

#include <exception>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <cassert>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <variant>
#include <cassert>

namespace cobra {

	http_filter::http_filter(std::shared_ptr<const config::config> config, std::size_t match_count)
		: _config(config), _match_count(match_count + config->location.size()) {
		for (const auto& sub_filter : _config->sub_configs) {
			_sub_filters.push_back(http_filter(sub_filter, _match_count));
		}
	}

	http_filter::http_filter(std::shared_ptr<const config::config> config) : http_filter(config, 0) {}

	http_filter::http_filter(std::shared_ptr<const config::config> config, std::vector<http_filter> filters)
		: _config(config), _sub_filters(std::move(filters)) , _match_count(0) {}

	generator<std::pair<http_filter*, uri_abs_path>>
	http_filter::match(const basic_socket_stream& socket, const http_request& request, const uri_abs_path& normalized) {
		if (eval(socket, request, normalized)) {
			for (auto& filter : _sub_filters) {
				for (auto& result : filter.match(socket, request, normalized)) {
					co_yield std::move(result);
				}
			}
			
			co_yield { this, normalized };

			if (config().index.has_value()) {
				uri_abs_path normalized_clone = normalized;
				normalized_clone.push_back(config().index->string());

				for (auto& filter : _sub_filters) {
					for (auto& result : filter.match(socket, request, normalized_clone)) {
						co_yield std::move(result);
					}
				}

				co_yield { this, normalized_clone };
			}
		}
	}

	bool http_filter::eval(const basic_socket_stream& socket, const http_request& request, const uri_abs_path& normalized) const {
		if (!config().server_names.empty()) {
			if (socket.server_name()) {
				if (!config().server_names.contains(std::string(*socket.server_name()))) {
					return false;
				}
			} else {
				if (!request.has_header("host")) {
					return false;
				}
				if (!config().server_names.contains(request.header("host"))) {
					return false;
				}
			}
		}

		if (!config().location.empty()) {
			const std::size_t already_matched = match_count() - config().location.size();

			assert(already_matched <= static_cast<std::size_t>(std::distance(normalized.begin(), normalized.end())));
			auto it = normalized.begin() + already_matched;

			for (auto& part : config().location) {
					if (it == normalized.end()) {
						return false;
					} else if (*it != part) {
						return false;
					}
				++it;
			}
		}

		if (!config().methods.empty() && !config().methods.contains(request.method())) {
			return false;
		}

		if (!config().extensions.empty()) {
			bool matched = false;

			if (!normalized.empty()) {
				for (const std::string& extension : config().extensions) {
					if (normalized.back().ends_with(extension)) {
						matched = true;
						break;
					}
				}
			}

			if (!matched) {
				return false;
			}
		}

		return true;
	}

	server::server(config::listen_address address, std::unordered_map<std::string, ssl_ctx> contexts,
				   std::vector<http_filter> filters, executor* exec, event_loop* loop)
		: http_filter(std::shared_ptr<config::config>(new config::config()), std::move(filters)),
		  _address(std::move(address)), _contexts(std::move(contexts)), _exec(exec), _loop(loop) {}

	server::server(server&& other)
		: http_filter(std::move(other)), _address(std::move(other._address)), _contexts(std::move(other._contexts)),
		  _exec(other._exec), _loop(other._loop), _num_connections(other._num_connections.load()) {}

	task<void> server::match_and_handle(basic_socket_stream& socket, const http_request& request,
										buffered_istream_reference in, http_ostream_wrapper& out,
										http_server_logger* logger, std::optional<http_response_code> code) {
		//std::optional<std::pair<int, const config::config*>> error;
		const uri_origin* org = request.uri().get<uri_origin>();

		if (!org) {
			eprintln("request did not contain uri_origin");
			throw HTTP_BAD_REQUEST;
		}

		const config::config* last_config = nullptr;//ODOT give other name

		const uri_abs_path normalized = org->path().normalize();
		for (auto [filter, normalized] : match(socket, request, normalized)) {
			try {
				if (!filter || !filter->config().handler) {
					/*
					if (!filter)
						std::cerr << "no filter matched" << std::endl;
					else if (!filter->config().handler)
						std::cerr << "no handler" << std::endl;
					*/
					continue;
				}

				if (last_config == nullptr)
					last_config = &filter->config();

				// co_await (*handler)(std::move(writer), std::move(request), buffered_istream_reference(socket_istream));
				http_response_writer writer(&request, &out, logger);
				co_await handle_request(*filter, request, normalized, in, std::move(writer), code);
				co_return;
			} catch (int code) {
				last_config = &filter->config();
				if (code != HTTP_NOT_FOUND) {
					throw std::make_pair(code, &filter->config());
				}
			} catch (const std::exception& ex) {
				eprintln("An internal server error has ocurred: {}", ex.what());
				throw std::make_pair(HTTP_INTERNAL_SERVER_ERROR, &filter->config());
				//error = std::make_pair(HTTP_INTERNAL_SERVER_ERROR, &config());
			} catch (...) {
				eprintln("An exception was thrown that does not inherit from std::exception");
				throw std::make_pair(HTTP_INTERNAL_SERVER_ERROR, &filter->config());
			}
		}
		throw std::make_pair(HTTP_NOT_FOUND, last_config);
	}

	task<void> server::on_connect(basic_socket_stream& socket) {
		istream_buffer socket_istream(make_istream_ref(socket), COBRA_BUFFER_SIZE);
		ostream_buffer socket_ostream(make_ostream_ref(socket), COBRA_BUFFER_SIZE);
		http_ostream_wrapper wrapper(socket_ostream);
		http_server_logger logger;
		logger.set_socket(socket);

		counter c(_num_connections);

		if (false && c.prev_val() >= max_connections()) {
			co_await socket_istream.fill_buf();
			http_response response(HTTP_SERVICE_UNAVAILABLE);
			response.set_header("Retry-After", "10");
			co_await http_response_writer(nullptr, &wrapper, &logger).send(std::move(response));
			co_await wrapper.end();
		} else {
			http_request request("GET", parse_uri("/", "GET"));
			do {
				std::optional<std::pair<int, const config::config*>> error;

				try {
					try {
						request = co_await parse_http_request(socket_istream);
					} catch (http_parse_error err) {
						throw HTTP_BAD_REQUEST;
					} catch (uri_parse_error err) {
						throw HTTP_BAD_REQUEST;
					}

					co_await match_and_handle(socket, request, socket_istream, wrapper, &logger);
				} catch (std::pair<int, const config::config*> err) {
					error = err;
					eprintln("http error {}", err.first);
					//error = code;
				}

				if (!wrapper.sent() && error.has_value() && error->second &&
					error->second->error_pages.contains(error->first)) {
					const std::string& error_page = error->second->error_pages.at(error->first);
					std::string uri;
					if (!error_page.empty()) {
						if (error_page[0] == '/') {
							uri = error_page;
						} else {
							eprintln("this should probably do something different");
							uri = error_page;
						}
					}

					http_request error_request("GET", parse_uri(std::move(uri), "GET"));
					if (request.has_header("host")) {
						error_request.add_header("host", request.header("host"));
					}

					bool errored_again = false;
					try {
						co_await match_and_handle(socket, error_request, socket_istream, wrapper, &logger, error->first);
					} catch (...) {
						errored_again = true;
					}

					if (errored_again) {
						co_await http_response_writer(&request, &wrapper, &logger).send(HTTP_NOT_FOUND);
					}
				} else if (!wrapper.sent() && error.has_value()) {
					// eprintln("no error_page ({}) {}", error->first, error->second->error_pages.contains(error->first));
					co_await http_response_writer(&request, &wrapper ,nullptr).send(error->first);
				}

				co_await wrapper.end();
			} while (wrapper.keep_alive());
		}
	}

	task<void> server::handle_request(const http_filter& filt, const http_request& request, const uri_abs_path& normalized,
									  buffered_istream_reference in, http_response_writer writer, std::optional<http_response_code> code) {
		for (auto& [key, value] : filt.config().headers) {
			writer.set_header(key, value);
		}

		// ODOT properly match uri
		fs::path file("/");
		for (std::size_t i = filt.match_count(); i < normalized.size(); ++i) {
			file.append(normalized[i]);
		}

		// NOTE do properly: https://datatracker.ietf.org/doc/html/rfc9112#name-message-body-length
		auto content_length = request.has_header("content-length")
								  ? parse_unsigned_strict<std::size_t>(request.header("content-length"))
								  : 0;
		if (!content_length)
			throw HTTP_BAD_REQUEST;

		gluttonous_stream limited_stream(istream_limit(std::move(in), *content_length), filt.config().max_body_size);
		
		if (!has_header_value(request, "Connection", "Upgrade") || !has_header_value(request, "Upgrade", "websocket")) {
			in = limited_stream;
		}

		//ODOT do without allocations
		auto root = filt.config().root.value_or("").string();
		auto index = std::vector<std::string>(1, filt.config().index.value_or("").string());

		if (auto cfg = std::get_if<cobra::cgi_config>(&*filt.config().handler)) {
			co_await handle_cgi(std::move(writer), {_loop, _exec, root, file.string(), *cfg, request, in});
		} else if (auto cfg = std::get_if<cobra::static_config>(&*filt.config().handler)) {
			co_await handle_static(std::move(writer), {_loop, _exec, root, file.string(), *cfg, request, in}, code);
		} else if (auto cfg = std::get_if<cobra::redirect_config>(&*filt.config().handler)) {
			co_await handle_redirect(std::move(writer), {_loop, _exec, root, file.string(), *cfg, request, in});
		} else if (auto cfg = std::get_if<cobra::proxy_config>(&*filt.config().handler)) {
			co_await handle_proxy(std::move(writer), {_loop, _exec, root, file.string(), *cfg, request, in});
		} else {
			assert(0 && "Unimplemented");
		}
	}

	// TODO: disable ssl in mandatory
	task<void> server::start(executor* exec, event_loop* loop) {
		std::string service = std::to_string(_address.service());
		if (_contexts.empty()) {
			std::cout << _address.node() << ":" << service << std::endl;
			co_return co_await start_server(exec, loop, _address.node().data(), service.c_str(),
											[this](socket_stream socket) -> task<void> {
												co_return co_await on_connect(socket);
											});
		} else if (_contexts.size() == 1 && _contexts.begin()->first.empty()) {
			//No SNI
			eprintln("ssl {}:{}", _address.node(), service);
			co_return co_await start_ssl_server(_contexts.begin()->second, exec, loop, _address.node().data(),
												service.c_str(), [this](ssl_socket_stream socket) -> task<void> {
													co_return co_await on_connect(socket);
												});
		} else {
			//With SNI
			eprintln("ssl {}:{} SNI", _address.node(), service);
			co_return co_await start_ssl_server(_contexts, exec, loop, _address.node().data(),
												service.c_str(), [this](ssl_socket_stream socket) -> task<void> {
													co_return co_await on_connect(socket);
												});
		}
	}

	std::vector<server> server::convert(const std::vector<std::shared_ptr<config::server>>& configs,
										executor* exec, event_loop* loop) {
		std::map<config::listen_address, std::unordered_map<std::string, ssl_ctx>> contexts;
		std::map<config::listen_address, std::vector<http_filter>> filters;

		for (const auto& config : configs) {
			for (const auto& address : config->addresses) {
				filters[address].push_back(http_filter(config));
				if (config->ssl) {
					if (config->server_names.empty()) {
						//Only one server
						contexts[address].insert({"", ssl_ctx::server(config->ssl->cert(), config->ssl->key())});
					} else {
						for (const auto& server_name : config->server_names) {
							contexts[address].insert({server_name, ssl_ctx::server(config->ssl->cert(), config->ssl->key())});
						}
					}
				}
			}

		}

		std::vector<server> result;
		result.reserve(filters.size());
		for (const auto& [listen, filters] : filters) {
			std::unordered_map<std::string, ssl_ctx> ssl;
			if (contexts.contains(listen)) {
				ssl = contexts.at(listen);
			}
			result.push_back(server(listen, std::move(ssl), filters, exec, loop));
		}
		return result;
	}
} // namespace cobra
