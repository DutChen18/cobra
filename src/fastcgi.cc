#include "cobra/fastcgi.hh"

#include "cobra/print.hh"
#include "cobra/serde.hh"

namespace cobra {
	fastcgi_client_connection::fastcgi_client_connection(istream_reference istream, ostream_reference ostream)
		: _istream(istream), _ostream(ostream) {}

	async_mutex& fastcgi_client_connection::mutex() {
		return _mutex;
	}

	async_condition_variable& fastcgi_client_connection::condition_variable() {
		return _condition_variable;
	}

	task<std::shared_ptr<fastcgi_client>> fastcgi_client_connection::get_client(std::uint16_t request_id) {
		async_lock lock = co_await async_lock::lock(_mutex);
		co_return _clients.at(request_id);
	}

	task<void> fastcgi_client_connection::write_header(fastcgi_record_type type, std::uint16_t request_id,
													   std::uint16_t content_length) {
		co_await write_u8(_ostream, FCGI_VERSION_1);
		co_await write_u8(_ostream, static_cast<std::uint8_t>(type));
		co_await write_u16_be(_ostream, request_id);
		co_await write_u16_be(_ostream, content_length);
		co_await write_u8(_ostream, 0);
		co_await write_u8(_ostream, 0);
	}

	task<std::size_t> fastcgi_client_connection::write(fastcgi_client* client, fastcgi_record_type type,
													   const char* data, std::size_t size) {
		size = std::min(size, std::size_t(65535));

		if (size > 0) {
			async_lock lock = co_await async_lock::lock(_mutex);

			if (_clients.at(client->request_id()).get() == client) {
				co_await write_header(type, client->request_id(), size);
				co_await _ostream.write_all(data, size);
			}
		}

		co_return size;
	}

	task<void> fastcgi_client_connection::flush(fastcgi_client* client, fastcgi_record_type type) {
		async_lock lock = co_await async_lock::lock(_mutex);
		co_await _ostream.flush();
		(void)client;
		(void)type;
	}

	task<void> fastcgi_client_connection::close(fastcgi_client* client, fastcgi_record_type type) {
		async_lock lock = co_await async_lock::lock(_mutex);

		if (_clients.at(client->request_id()).get() == client) {
			co_await write_header(type, client->request_id(), 0);
			co_await _ostream.flush();
		}
	}

	task<void> fastcgi_client_connection::abort(fastcgi_client* client) {
		async_lock lock = co_await async_lock::lock(_mutex);

		if (_clients.at(client->request_id()).get() == client) {
			co_await write_header(fastcgi_record_type::fcgi_abort_request, client->request_id(), 0);
			co_await _ostream.flush();
		}
	}

	task<std::shared_ptr<fastcgi_client>> fastcgi_client_connection::begin() {
		async_lock lock = co_await async_lock::lock(_mutex);
		std::uint16_t request_id = 1;

		for (const auto& [key, value] : _clients) {
			if (key == request_id) {
				request_id += 1;
			} else {
				break;
			}
		}

		std::shared_ptr client = std::make_shared<fastcgi_client>(request_id, *this);
		_clients.emplace(request_id, client);
		co_await write_header(fastcgi_record_type::fcgi_begin_request, request_id, 8);
		co_await write_u16_be(_ostream, FCGI_RESPONDER);
		co_await write_u8(_ostream, FCGI_KEEP_CONN);
		co_await write_u8(_ostream, 0);
		co_await write_u32_be(_ostream, 0);
		co_await _ostream.flush();
		co_return client;
	}

	task<bool> fastcgi_client_connection::poll() {
		std::uint8_t version = co_await read_u8(_istream);
		std::uint8_t type = co_await read_u8(_istream);
		std::uint16_t request_id = co_await read_u16_be(_istream);
		std::uint16_t content_length = co_await read_u16_be(_istream);
		std::uint8_t padding_length = co_await read_u8(_istream);
		co_await read_u8(_istream);

		if (version != FCGI_VERSION_1) {
			std::shared_ptr<fastcgi_client> client = co_await get_client(request_id);
			co_await client->set_error(fastcgi_error::bad_version);
		}

		if (type == static_cast<std::uint8_t>(fastcgi_record_type::fcgi_end_request)) {
			std::shared_ptr<fastcgi_client> client = co_await get_client(request_id);
			co_await read_u32_be(_istream);
			std::uint8_t protocol_status = co_await read_u8(_istream);
			co_await read_u8(_istream);
			co_await read_u16_be(_istream);
			co_await client->fcgi_stdout().close();
			co_await client->fcgi_stderr().close();

			if (protocol_status == FCGI_CANT_MPX_CONN) {
				co_await client->set_error(fastcgi_error::cant_mpx_conn);
			} else if (protocol_status == FCGI_OVERLOADED) {
				co_await client->set_error(fastcgi_error::overloaded);
			} else if (protocol_status == FCGI_UNKNOWN_ROLE) {
				co_await client->set_error(fastcgi_error::unknown_role);
			} else if (protocol_status != FCGI_REQUEST_COMPLETE) {
				co_await client->set_error(fastcgi_error::unknown_error);
			}

			async_lock lock = co_await async_lock::lock(_mutex);
			_clients.erase(request_id);
		} else if (type == static_cast<std::uint8_t>(fastcgi_record_type::fcgi_stdout)) {
			std::vector<char> buffer;
			buffer.resize(content_length);
			co_await _istream.read_all(buffer.data(), content_length);
			std::shared_ptr<fastcgi_client> client = co_await get_client(request_id);
			co_await client->fcgi_stdout().write(buffer.data(), content_length);
		} else if (type == static_cast<std::uint8_t>(fastcgi_record_type::fcgi_stderr)) {
			std::vector<char> buffer;
			buffer.resize(content_length);
			co_await _istream.read_all(buffer.data(), content_length);
			std::shared_ptr<fastcgi_client> client = co_await get_client(request_id);
			co_await client->fcgi_stderr().write(buffer.data(), content_length);
		} else {
			std::vector<char> buffer;
			buffer.resize(content_length);
			co_await _istream.read_all(buffer.data(), content_length);
		}

		std::vector<char> buffer;
		buffer.resize(padding_length);
		co_await _istream.read_all(buffer.data(), padding_length);
		async_lock lock = co_await async_lock::lock(_mutex);
		co_return !_clients.empty();
	}

	fastcgi_client::fastcgi_client(std::uint16_t request_id, fastcgi_client_connection& connection)
		: _request_id(request_id), _connection(&connection) {}

	std::uint16_t fastcgi_client::request_id() const {
		return _request_id;
	}

	fastcgi_client_connection* fastcgi_client::connection() const {
		return _connection;
	}

	task<void> fastcgi_client::set_error(fastcgi_error error) {
		async_lock lock = co_await async_lock::lock(_connection->mutex());
		_error = error;
		_connection->condition_variable().notify_all();
	}

	void fastcgi_client::check_error() const {
		if (_error) {
			throw *_error;
		}
	}

	fastcgi_ostream<fastcgi_record_type::fcgi_params>& fastcgi_client::fcgi_params() {
		return *this;
	}

	fastcgi_ostream<fastcgi_record_type::fcgi_stdin>& fastcgi_client::fcgi_stdin() {
		return *this;
	}

	fastcgi_istream<fastcgi_record_type::fcgi_stdout>& fastcgi_client::fcgi_stdout() {
		return *this;
	}

	fastcgi_istream<fastcgi_record_type::fcgi_stderr>& fastcgi_client::fcgi_stderr() {
		return *this;
	}

	fastcgi_ostream<fastcgi_record_type::fcgi_data>& fastcgi_client::fcgi_data() {
		return *this;
	}
} // namespace cobra
