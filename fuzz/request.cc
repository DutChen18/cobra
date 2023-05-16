#include "cobra/asio.hh"
#include "cobra/context.hh"
#include "cobra/future.hh"
#include "cobra/http.hh"
#include "cobra/executor.hh"
#include "cobra/event_loop.hh"
#include <memory>

#ifdef COBRA_FUZZ

extern "C" {
#include <stdint.h>
#include <stddef.h>
}

namespace cobra {
	class istringstream : public basic_istream<char> {
		const uint8_t *_data;
		const uint8_t *_end;

	public:
		istringstream(const uint8_t *data, size_t size) : _data(data), _end(data + size) {}

		future<std::size_t> read(char* dst, size_t count) override {
			std::size_t left = _end - _data;
			std::size_t read = count < left ? count : left;
			if (read == 0) {
				return resolve(std::move(read));
			}

			std::memcpy(dst, _data, read);
			_data += read;
			return resolve(std::move(read));
		}
	};
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	std::unique_ptr<cobra::context> ctx = cobra::default_context();
	std::shared_ptr<cobra::istringstream> istream = std::make_shared<cobra::istringstream>(cobra::istringstream(Data, Size));
	cobra::buffered_istream stream(istream);

	cobra::parse_request(stream).start_later(*ctx, [](cobra::future_result<cobra::http_request>) {});

	ctx->run_until_complete();
	return 0;
}
#endif
