#ifndef COBRA_COMPRESS_LZ_HH
#define COBRA_COMPRESS_LZ_HH

#ifdef COBRA_DEBUG
#define FORCE_INLINE __attribute__((always_inline))
#else
#define FORCE_INLINE
#endif

#include "cobra/asyncio/task.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/ringbuffer.hh"
#include "cobra/chained_iterator.hh"

#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <unordered_map>
#include <algorithm>

//TODO remove these headers
#include <iostream>
#include <set>
#include <vector>
#include "cobra/print.hh"
#include "cobra/compress/stream_ringbuffer.hh"

namespace cobra {

	class lz_command {
		uint16_t _length;
		union {
			uint16_t _distance;
			uint8_t _character;
		};

	public:
		constexpr explicit lz_command(uint8_t ch) noexcept : _length(1), _character(ch) {}
		constexpr lz_command(uint16_t length, uint16_t dist) noexcept : _length(length), _distance(dist) {}
		constexpr lz_command(const lz_command& other) noexcept = default;

		constexpr lz_command& operator=(const lz_command& other) noexcept = default;

		constexpr bool operator==(const lz_command& other) const noexcept {
			if (is_literal() && other.is_literal())
				return ch() == other.ch();
			return length() == other.length() && dist() == other.dist();
		}

		inline constexpr bool is_literal() const noexcept { return _length == 1; }
		inline constexpr uint16_t length() const noexcept { return _length; }
		inline constexpr uint16_t dist() const noexcept { return _distance; }
		inline constexpr uint8_t ch() const noexcept { return _character; }
	};

	class zchain {
	public:
		using buffer_iterator = ringbuffer<uint8_t>::iterator;

		class zchain_iterator {
		public:
			using iterator_tag = std::forward_iterator_tag;

		private:
			zchain* _current = nullptr;

		public:
			constexpr zchain_iterator() noexcept = default;
			constexpr zchain_iterator(zchain* chain) noexcept : _current(chain) {}
			constexpr zchain_iterator(zchain_iterator&& other) noexcept
				: _current(std::exchange(other._current, nullptr)) {}
			constexpr zchain_iterator(const zchain_iterator& other) noexcept = default;
			constexpr ~zchain_iterator() {
				_current = nullptr;
			}

			constexpr zchain_iterator& operator=(const zchain_iterator& other) noexcept = default;
			constexpr zchain_iterator& operator=(zchain_iterator&& other) noexcept {
				if (this != &other) {
					swap(other);
				}
				return *this;
			}

			constexpr bool operator==(const zchain_iterator& other) const noexcept = default;
			constexpr bool operator!=(const zchain_iterator& other) const noexcept = default;

			inline constexpr zchain& operator*() noexcept {
				return *_current;
			};

			inline constexpr const zchain& operator*() const noexcept {
				return *_current;
			}

			inline constexpr zchain* operator->() noexcept {
				return _current;
			}

			inline constexpr const zchain* operator->() const noexcept {
				return _current;
			}

			inline constexpr zchain_iterator& operator++() noexcept {
				_current = _current->_next;
				return *this;
			}

			inline constexpr zchain_iterator operator++(int) noexcept {
				zchain_iterator tmp(*this);
				_current = _current->_next;
				return tmp;
			}

			inline constexpr void swap(zchain_iterator& other) noexcept {
				std::swap(_current, other._current);
			}
		};

		using iterator = zchain_iterator;
		using const_iterator = const zchain_iterator;
	
	private:
		uint32_t _hash;
		zchain* _next = nullptr;
		buffer_iterator _pos;
	
	public:
		constexpr zchain(uint32_t hash, buffer_iterator pos) noexcept : _hash(hash), _pos(pos) {}
		constexpr zchain(zchain&& other) noexcept
			: _hash(std::exchange(other._hash, std::numeric_limits<uint32_t>::max())),
			  _next(std::exchange(other._next, nullptr)), _pos(std::move(other._pos)) {}
		constexpr ~zchain() {
			_hash = std::numeric_limits<uint32_t>::max();
			_next = nullptr;
			_pos = buffer_iterator();
		}
		//constexpr zchain(const zchain& other) noexcept = default;

		inline constexpr iterator begin() noexcept { return iterator(this); }
		inline constexpr iterator end() noexcept { return iterator(); }
		inline constexpr iterator next() noexcept { return iterator(_next); }
		inline constexpr const_iterator next() const noexcept { return iterator(_next); }

		inline constexpr zchain& operator=(zchain&& other) noexcept {
			if (this != &other) {
				swap(other);
			}
			return *this;
		}

		constexpr bool operator==(const zchain& other) const noexcept = default;

		inline constexpr bool operator==(uint32_t hash) const noexcept {
			return _hash == hash;
		}

		inline constexpr uint32_t hash() const noexcept {
			return _hash;
		}

		inline constexpr buffer_iterator pos() noexcept {
			return _pos;
		}

		inline constexpr zchain* set_next(zchain* next) noexcept {
			return std::exchange(_next, next);
		}

		inline constexpr zchain* set_next(zchain& next) noexcept {
			return set_next(&next);
		}

		constexpr void swap(zchain& other) noexcept {
			std::swap(_hash, other._hash);
			std::swap(_next, other._next);
			_pos.swap(other._pos);
		}
	};

	inline constexpr void swap(zchain& lhs, zchain& rhs) noexcept {
		lhs.swap(rhs);
	}

	inline constexpr void swap(zchain::zchain_iterator& lhs, zchain::zchain_iterator& rhs) noexcept {
		lhs.swap(rhs);
	}

	class lz_debug_istream {
		ringbuffer<uint8_t> _window;

	public:
		lz_debug_istream(std::size_t window_size) : _window(window_size) {}

		task<void> write(const lz_command& command) {
			if (command.is_literal()) {
				println("{}({})", (char) command.ch(), command.ch());
			} else {
				println("({},{})", command.dist(), command.length());
			}
			co_return;
		}

		task<void> flush() {
			co_return;
		}
	};

	class lz_istream : public basic_istream_ringbuffer<lz_istream, char> {
		using base = basic_istream_ringbuffer<lz_istream, char>;
		std::deque<lz_command> _commands;
		std::size_t ncopied = 0;

	public:
		lz_istream(std::size_t window_size) : base(window_size) {}

		task<void> write(const lz_command& command) {
			_commands.push_back(command);
			co_return;
		}

		task<void> fill_ringbuf() {
			auto it = _commands.begin();

			while (!_commands.empty()) {
				auto& command = *it++;

				if (command.is_literal()) {
					char ch = command.ch();
					if (base::write(&ch, 1) == 0) {
						break;
					}
					_commands.pop_front();
				} else {
					ncopied += base::copy(command.dist(), command.length() - ncopied);

					if (ncopied < command.length()) {
						break;
					} else {
						ncopied = 0;
						_commands.pop_front();
					}
				}
			}
			co_return;
		}

		task<void> flush() {
			co_return;
		}
	};

	template <class Stream>
	class lz_ostream : public ostream_impl<lz_ostream<Stream>> {
	public:
		using base_type = ostream_impl<lz_ostream<Stream>>;
		using typename base_type::char_type;

	private:
		Stream _stream;
		ringbuffer<uint8_t> _buffer;
		ringbuffer<uint8_t> _window;

		ringbuffer<zchain> _chain;
		std::unordered_map<uint32_t, std::pair<zchain::iterator, zchain::iterator>> _table;

		std::size_t _min_backref_length = 3;
		std::size_t _max_backref_length = 258;

	public:
		lz_ostream(Stream&& stream, std::size_t window_size)
			: _stream(std::move(stream)), _buffer(window_size), _window(window_size),
			  _chain(window_size), _table() {}

		lz_ostream(lz_ostream&& other)
			: _stream(std::move(other._stream)), _buffer(std::move(other._buffer)), _window(std::move(other._window)),
			  _chain(std::move(other._chain)), _table(std::move(other._table)) {}

		lz_ostream& operator=(lz_ostream&& other) noexcept {
			if (this != &other) {
				std::swap(_stream, other._stream);
				swap(_buffer, other._buffer);
				swap(_window, other._window);
				swap(_chain, other._chain);
				std::swap(_table, other._table);
			}
			return *this;
		}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			const std::size_t result = size;
			while (size > 0) {
				const std::size_t n = std::min(_buffer.remaining(), size);
				_buffer.insert(data, data + n);
				data += n;
				size -= n;
				co_await produce_one();
			}
			co_return result;
		}

		task<Stream> end() && {
			co_await produce_atleast(_buffer.size());
			co_return std::move(_stream);
		}

		task<void> flush() {
			co_await produce_atleast(_buffer.size());
			co_await _stream.flush();
		}

	private:
#ifdef COBRA_DEBUG_
		void assert_table_correct() {
			for (auto& [key, value] : _table) {
				for (auto& link : *value.first) {
					assert(link.hash() == key);
					if (std::distance(link.pos(), _window.end()) >= 3) {
						uint32_t actual_hash = calc_hash(*(link.pos() + 2), *(link.pos() + 1), *link.pos());
						assert(actual_hash == key);
					}
				}
				assert(value.first != _chain[0].end());
				assert(value.second != _chain[0].end());

				if (value.first->next() == nullptr)
					assert(value.first == value.second);
				if (value.first == value.second)
					assert(value.first->next() == nullptr);

				auto it = value.first->begin();
				while (true) {
					assert(it != _chain[0].end());
					if (it++ == value.second) {
						assert(it == _chain[0].end());
						break;
					}
				}
			}

		}

		void assert_chain_correct() {
			std::set<uint32_t> checked;
			for (auto& link : _chain) {
				assert(_table.find(link.hash()) != _table.end());
				if (!checked.contains(link.hash())) {
					assert(_table.at(link.hash()).first == link.begin());
					checked.insert(link.hash());
				}
			}
		}

		void assert_correct() {
			assert_table_correct();
			assert_chain_correct();
		}
#else
		FORCE_INLINE void assert_table_correct() {}
		FORCE_INLINE void assert_chain_correct() {}
		FORCE_INLINE void assert_correct() {}
#endif

		task<void> produce_one() {
			assert_correct();
			assert(!_buffer.empty());
			if (_buffer.size() < _min_backref_length) {
				//Not enough buffered to produce a backreference or to store a hash
				co_await write_literal_command_from_buffer();
				co_return;
			}

			const auto hash = peek_hash();
			const auto it = _table.find(hash);

			if (it == _table.end()) {
				//Did not find anything in the window that starts with the same characters
				co_await write_literal_command_chained_from_buffer(hash);
				co_return;
			}

			//Found backreference
			auto [first, _] = it->second;
			
			std::size_t best_length = _min_backref_length;
			zchain* best_link = &*first;

			assert_correct();
			for (auto& link : *first) {
				auto [win_it, buf_it] = std::mismatch(link.pos(), _window.end(), _buffer.begin(), _buffer.end()); 
				std::size_t length = buf_it - _buffer.begin();

				if (win_it == _window.end()) {
					auto [_, a] = std::mismatch(_buffer.begin(), _buffer.end(), buf_it, _buffer.end());
					length = a - _buffer.begin();
				}

				if (length >= best_length) {
					best_link = &link;
					best_length = std::min(length, _max_backref_length);

					if (length >= _max_backref_length)
						break;
				}
			}
			assert(best_link);

			const std::size_t window_match = _window.end() - best_link->pos();
			co_await write_copy_command(hash, best_length, window_match);
		}

		auto remove_oldest_link() {
			const auto it = _chain.begin();
			const auto res = it + 1;
			const uint32_t link_hash = it->hash();

			auto table_it = _table.find(link_hash);
			if (table_it == _table.end())
				return res;

			const auto [head, tail] = table_it->second;
			assert(it->next() == head->next());
			assert(it->hash() == head->hash() && it->hash() == tail->hash());

			if (head->next() == nullptr)
				assert(head == tail);

			if (head == tail) {
				//Was the last link
				_table.erase(link_hash);
			} else {
				_table[link_hash] = {it->next(), tail};
			}

			_chain.pop_front();
			assert_correct();
			return res;
		}

		auto write_literal(uint8_t ch) {
			if (_window.full() || _chain.full()) {
				remove_oldest_link();
			}
			return _window.push_back(ch);
		}

		task<ringbuffer<uint8_t>::iterator> write_literal_command(uint8_t ch) {
			co_await _stream.write(lz_command(ch));
			co_return write_literal(ch);
		}

		task<void> write_literal_command_from_buffer() {
			co_await write_literal_command(_buffer.pop_front());
		}

		void update_table(uint32_t hash, zchain::iterator it) {
			auto table_it = _table.find(hash);
			if (table_it == _table.end()) {
				_table[hash] = {it, it};
			} else {
				table_it->second.second->set_next(&*it);
				table_it->second.second = it;
			}
		}

		void update_table(uint32_t hash, ringbuffer<zchain>::iterator it) {
			update_table(hash, it->begin());
		}

		task<void> write_literal_command_chained(uint8_t ch, uint32_t hash) {
			assert_correct();
			const auto win_it = co_await write_literal_command(ch);

			assert(!_chain.full());
			const auto chain_it = _chain.push_back(zchain(hash, win_it));
			assert_table_correct();

			update_table(hash, chain_it);
			assert_correct();
			co_return;
		}

		task<void> write_literal_command_chained_from_buffer(uint32_t hash) {
			co_await write_literal_command_chained(_buffer.pop_front(), hash);
		}

		auto write_from_buffer(std::size_t count) {
			auto it = _window.insert(_buffer.begin(), _buffer.begin() + count);
			_buffer.erase_front(count);
			return it;
		}

		task<void> write_copy_command(uint32_t hash, uint16_t length, uint16_t dist) {
			assert_correct();
			if (_window.remaining() <= length) {
				auto chain_it = _chain.begin();

				const auto buffer_replace_end = _window.begin() + length - _window.remaining();
				while (chain_it != _chain.end() && chain_it->pos() <= buffer_replace_end) {
					chain_it = remove_oldest_link();
				}
			}
			assert_correct();

			if (_chain.full()) {
				remove_oldest_link();
			}
			assert_correct();

			//std::size_t n = std::min(length, static_cast<uint16_t>(_buffer.size()));
			const auto win_it =_window.insert(_buffer.begin(), _buffer.begin() + length);
			_buffer.erase_front(length);

			const auto chain_it = _chain.push_back(zchain(hash, win_it));
			update_table(hash, chain_it);
			assert_correct();
			co_await _stream.write(lz_command(length, dist));
		}

		uint32_t calc_hash(uint8_t l, uint8_t c, uint8_t r) {
			return static_cast<uint32_t>(l) << 16 | static_cast<uint32_t>(c) << 8 |  r;
		}

		uint32_t peek_hash() {
			return calc_hash(_buffer[2], _buffer[1], _buffer[0]);
			//return static_cast<uint32_t>(_buffer[2]) << 16 | static_cast<uint32_t>(_buffer[1]) << 8 |  _buffer[0];
		}

		task<void> produce_atleast(size_t at_least) {
			assert(at_least <= _buffer.size() && "tried to produce  more than available");
			const size_t before = _buffer.size();
			while (!_buffer.empty() && before - _buffer.size() <= at_least) {
				co_await produce_one();
			}
		}
	};
}
#endif
