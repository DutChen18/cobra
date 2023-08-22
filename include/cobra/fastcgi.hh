#ifndef COBRA_FASTCGI_HH
#define COBRA_FASTCGI_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/mutex.hh"
#include "cobra/net/stream.hh"

#include <cstdint>

#define FCGI_VERSION_1 1
#define FCGI_KEEP_CONN 1
#define FCGI_RESPONDER 1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER 3

namespace cobra {
	enum class fastcgi_record_type {
		fcgi_begin_request = 1,
		fcgi_abort_request = 2,
		fcgi_end_request = 3,
		fcgi_params = 4,
		fcgi_stdin = 5,
		fcgi_stdout = 6,
		fcgi_stderr = 7,
		fcgi_data = 8,
		fcgi_get_values = 9,
		fcgi_get_values_result = 10,
		fcgi_unknown_type = 11,
	};

	template <fastcgi_record_type Type>
	class fastcgi_istream : public istream_impl<fastcgi_istream<Type>> {
	public:
		using typename istream_impl<fastcgi_istream<Type>>::char_type;

	private:
		std::deque<char_type> _data;
		bool _closed = false;

	public:
		task<std::size_t> read(char_type* data, std::size_t size);
		task<void> write(const char_type* data, std::size_t size);
		task<void> close();
	};

	template <fastcgi_record_type Type>
	class fastcgi_ostream : public ostream_impl<fastcgi_ostream<Type>> {
	public:
		using typename ostream_impl<fastcgi_ostream<Type>>::char_type;

		task<std::size_t> write(const char_type* data, std::size_t size);
		task<void> flush();
		task<void> close();
	};

	class fastcgi_client;

	// TODO: reuse connections
	class fastcgi_client_connection {
		async_mutex _mutex;
		async_condition_variable _condition_variable;
		istream_reference _istream;
		ostream_reference _ostream;
		// TODO: only remove client from list after caller is done with it
		std::map<std::uint16_t, std::shared_ptr<fastcgi_client>> _clients;

		task<std::shared_ptr<fastcgi_client>> get_client(std::uint16_t request_id);
		task<void> write_header(fastcgi_record_type type, std::uint16_t request_id, std::uint16_t content_length);

	public:
		fastcgi_client_connection(istream_reference istream, ostream_reference ostream);

		async_mutex& mutex();
		async_condition_variable& condition_variable();

		task<std::size_t> write(std::uint16_t request_id, fastcgi_record_type type, const char* data, std::size_t size);
		task<void> flush(std::uint16_t request_id, fastcgi_record_type type);
		task<void> close(std::uint16_t request_id, fastcgi_record_type type);

		task<std::shared_ptr<fastcgi_client>> begin();
		task<bool> poll();
	};

	class fastcgi_client : public fastcgi_ostream<fastcgi_record_type::fcgi_params>, public fastcgi_ostream<fastcgi_record_type::fcgi_stdin>, public fastcgi_istream<fastcgi_record_type::fcgi_stdout>, public fastcgi_istream<fastcgi_record_type::fcgi_stderr>, public fastcgi_ostream<fastcgi_record_type::fcgi_data> {
		std::uint16_t _request_id;
		fastcgi_client_connection* _connection;

	public:
		fastcgi_client(std::uint16_t request_id, fastcgi_client_connection& connection);

		std::uint16_t request_id() const;
		fastcgi_client_connection* connection() const;

		fastcgi_ostream<fastcgi_record_type::fcgi_params>& fcgi_params();
		fastcgi_ostream<fastcgi_record_type::fcgi_stdin>& fcgi_stdin();
		fastcgi_istream<fastcgi_record_type::fcgi_stdout>& fcgi_stdout();
		fastcgi_istream<fastcgi_record_type::fcgi_stderr>& fcgi_stderr();
		fastcgi_ostream<fastcgi_record_type::fcgi_data>& fcgi_data();
	};

	template <fastcgi_record_type Type>
	task<std::size_t> fastcgi_istream<Type>::read(typename fastcgi_istream<Type>::char_type* data, std::size_t size) {
		fastcgi_client* client = static_cast<fastcgi_client*>(this);
		async_lock lock = co_await async_lock::lock(client->connection()->mutex());

		while (_data.empty()) {
			if (_closed) {
				co_return 0;
			}

			co_await client->connection()->condition_variable().wait(lock);
		}

		size = std::min(_data.size(), size);
		auto end = _data.begin();
		std::advance(end, size);
		std::copy(_data.begin(), end, data);
		_data.erase(_data.begin(), end);
		co_return size;
	}

	template <fastcgi_record_type Type>
	task<void> fastcgi_istream<Type>::write(const typename fastcgi_istream<Type>::char_type* data, std::size_t size) {
		fastcgi_client* client = static_cast<fastcgi_client*>(this);
		async_lock lock = co_await async_lock::lock(client->connection()->mutex());
		_data.insert(_data.end(), data, data + size);
		client->connection()->condition_variable().notify_all();
	}

	template <fastcgi_record_type Type>
	task<void> fastcgi_istream<Type>::close() {
		fastcgi_client* client = static_cast<fastcgi_client*>(this);
		async_lock lock = co_await async_lock::lock(client->connection()->mutex());
		_closed = true;
		client->connection()->condition_variable().notify_all();
	}

	template <fastcgi_record_type Type>
	task<std::size_t> fastcgi_ostream<Type>::write(const typename fastcgi_ostream<Type>::char_type* data, std::size_t size) {
		fastcgi_client* client = static_cast<fastcgi_client*>(this);
		return client->connection()->write(client->request_id(), Type, data, size);
	}

	template <fastcgi_record_type Type>
	task<void> fastcgi_ostream<Type>::flush() {
		fastcgi_client* client = static_cast<fastcgi_client*>(this);
		return client->connection()->flush(client->request_id(), Type);
	}

	template <fastcgi_record_type Type>
	task<void> fastcgi_ostream<Type>::close() {
		fastcgi_client* client = static_cast<fastcgi_client*>(this);
		return client->connection()->close(client->request_id(), Type);
	}
}

#endif
