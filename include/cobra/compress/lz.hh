#ifndef COBRA_COMPRESS_LZ_HH
#define COBRA_COMPRESS_LZ_HH

#include "cobra/asyncio/task.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/ringbuffer.hh"
#include "cobra/chained_iterator.hh"

#include <cstdint>
#include <iterator>
#include <utility>
#include <unordered_map>
#include <algorithm>

//TODO remove these headers
#include <iostream>
#include "cobra/print.hh"

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

		constexpr inline bool is_literal() const noexcept { return _length == 1; }
		constexpr inline uint16_t length() const noexcept { return _length; }
		constexpr inline uint16_t dist() const noexcept { return _distance; }
		constexpr inline uint8_t ch() const noexcept { return _character; }
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
			constexpr zchain_iterator(const zchain_iterator& other) noexcept = default;

			constexpr zchain_iterator& operator=(const zchain_iterator& other) noexcept = default;

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
		//constexpr zchain(const zchain& other) noexcept = default;

		constexpr inline iterator begin() noexcept { return iterator(this); }
		constexpr inline iterator end() noexcept { return iterator(); }
		constexpr inline iterator next() noexcept { return iterator(_next); }

		constexpr bool operator==(const zchain& other) const noexcept = default;

		inline constexpr bool operator==(uint32_t hash) const noexcept {
			return _hash == hash;
		}

		constexpr uint32_t hash() const noexcept {
			return _hash;
		}

		constexpr buffer_iterator pos() noexcept {
			return _pos;
		}

		constexpr zchain* set_next(zchain* next) noexcept {
			return std::exchange(_next, next);
		}

		constexpr zchain* set_next(zchain& next) noexcept {
			return set_next(&next);
		}
	};

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
	};

	template <class Stream>
	class lz_ostream : public ostream_impl<lz_ostream<Stream>> {
	public:
		using base_type = ostream_impl<lz_ostream<Stream>>;
		using typename base_type::char_type;

	private:
		Stream _stream;
		std::size_t _table_size;
		ringbuffer<uint8_t> _buffer;
		ringbuffer<uint8_t> _window;

		ringbuffer<zchain> _chain;
		std::unordered_map<uint32_t, std::pair<zchain::iterator, zchain::iterator>> _table;

		static constexpr std::size_t min_backref_length = 3;
		static constexpr std::size_t max_backref_length = (1 << 16) - 1;

	public:
		lz_ostream(Stream&& stream, std::size_t window_size)
			: _stream(std::move(stream)), _table_size(window_size), _buffer(_table_size), _window(_table_size),
			  _chain(window_size), _table() {}

		task<std::size_t> write(const char_type* data, const std::size_t size) {
			if (_buffer.remaining() >= size) {
				_buffer.insert(data, data + size);
			} else if (size < _buffer.size()) {
				co_await flush_atleast(size);
				co_return co_await write(data, size);
			} else {
				for (std::size_t index = 0; index < size; index += _buffer.capacity()) {
					co_await flush();
					_buffer.insert(data, data + std::min(size - index, _buffer.capacity()));
				}
			}
			co_return size;
		}

		task<Stream> end() && {
			co_await flush();
			co_return std::move(_stream);
		}

		task<void> flush() {
			co_return co_await flush_atleast(_buffer.size());
		}

	private:
		task<bool> produce_one() {
			if (_buffer.empty()) {
				//Nothing to produce
				co_return false;
			}
			if (_buffer.size() < min_backref_length) {
				//Not enough buffered to produce a backreference or to store a hash
				co_await write_literal();
				co_return true;
			}

			const auto hash = peek_hash();
			auto it = _table.find(hash);

			if (it == _table.end()) {
				//Did not find anything in the window that starts with the same characters

				if (_chain.full()) {
					//Remove the oldest link that's about to be overriden from the table
					remove_link(_chain.begin());
				}

				auto buffer_pos = co_await write_literal();
				auto chain_pos = _chain.push_back(zchain(hash, buffer_pos));
				_table[hash] = {chain_pos->begin(), chain_pos->begin()};
				co_return true;
			}

			//Found backreference
			auto [first, last] = it->second;
			
			std::size_t best_length = min_backref_length;
			zchain& best_link = *first;

			for (auto& link : *first) {
				//auto [win_it, buf_it] = std::mismatch(link.pos(), _window.end(), _buffer.begin(), _buffer.end()); 
				//std::size_t length = buf_it - _buffer.begin();

				chained_iterator cmp_it = chained_iterator(link.pos(), _window.end(), _buffer.begin(), _buffer.end());
				auto [win_it, buf_it] = std::mismatch(cmp_it.begin(), cmp_it.end(), _buffer.begin(), _buffer.end()); 
				std::size_t length = std::distance(buf_it, win_it.end());
				//std::size_t length = buf_it - _buffer.begin();

				if (length >= best_length) {
					best_link = link;
					best_length = std::min(length, max_backref_length);

					if (length >= max_backref_length)
						break;
				}
			}

			lz_command command(best_length, best_link.pos() - _window.begin() + best_length);

			if (_window.remaining() <= best_length) {
				//Remove all links that will have dangling references after insert
				auto chain_it = _chain.begin();
				auto buffer_replace_end = _window.begin() + best_length - _window.remaining();

				while (chain_it != _chain.end() && chain_it->pos() <= buffer_replace_end) {
					remove_link(chain_it);
				}

			}

			auto buffer_pos = write_from_buffer(best_length);
			auto chain_pos = _chain.push_back(zchain(hash, buffer_pos));
			last->set_next(*chain_pos);

			//Update lookup table
			_table[hash] = {first, chain_pos->begin()};

			co_await _stream.write(command);
			co_return true;
		}

		void remove_link(ringbuffer<zchain>::iterator it) {
			const uint32_t link_hash = it->hash();

			auto [head, tail] = _table[link_hash];

			if (head == tail) {
				//Was the last link
				_table.erase(link_hash);
			} else {
				_table[link_hash] = {it->next(), tail};
			}
		}

		task<ringbuffer<uint8_t>::iterator> write_literal() {
			const uint8_t ch = _buffer.pop_front();
			auto it = _window.push_back(ch);
			co_await _stream.write(lz_command(ch));
			co_return it;
		}

		auto write_from_buffer(std::size_t count) {
			auto it = _window.insert(_buffer.begin(), _buffer.begin() + count);
			_buffer.erase_front(count);
			return it;
		}

		uint32_t peek_hash() {
			return static_cast<uint32_t>(_buffer[2]) << 16 | static_cast<uint32_t>(_buffer[1]) << 8 |  _buffer[0];
		}

		task<void> flush_atleast(size_t at_least) {
			assert(at_least <= _buffer.size() && "tried to flush more than available");
			const size_t before = _buffer.size();
			while (before - _buffer.size() <= at_least) {
				if (!co_await produce_one())
					break;
			}
		}
	};
}
#endif
