#include <cstdint>
#include <memory>
#include <string>
#include <sstream>
#include "cobra/http/message.hh"
#include "cobra/http/parse.hh"
#include "cobra/asyncio/future_task.hh"
#include "cobra/asyncio/task.hh"
#include "cobra/asyncio/std_stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/net/address.hh"
#include "cobra/net/stream.hh"
#include "cobra/config.hh"
#include "cobra/http/server.hh"
#include <cstddef>
#include <fstream>
#include <cassert>
#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/executor.hh"

#ifdef COBRA_FUZZ_HANDLER

namespace cobra {
class fuzz_socket_stream : public cobra::basic_socket_stream {
        bool _shutdown_read = false;
        bool _shutdown_write = false;
        std::stringstream _in;
        std::stringstream& _out;
        address _addr;

public:
        fuzz_socket_stream(std::stringstream in, std::stringstream& out, address addr) : _in(std::move(in)), _out(out), _addr(addr) {}

        ~fuzz_socket_stream() {}

        task<std::size_t> read(char_type* data, std::size_t size) override {
                if (_shutdown_read) {
                        co_return 0;
                }
                co_return _in.readsome(data, size);
        }

        task<std::size_t> write(const char_type* data, std::size_t size) override {
                if (_shutdown_write) {
                        co_return 0;
                }
                _out << std::string_view(data, size);
                co_return size;
        }

        task<void> flush() override { co_return; }

        task<void> shutdown(cobra::shutdown_how how) override {
                switch (how) {
                case cobra::shutdown_how::read:
                        _shutdown_read = true;
                        break;
                case cobra::shutdown_how::write:
                        _shutdown_write = true;
                        break;
                case cobra::shutdown_how::both:
                        _shutdown_read = true;
                        _shutdown_write = true;
                        break;
                }
                co_return;
        }

        address peername() const override {
                return _addr;
        }

        std::optional<std::string_view> server_name() const override {
                return std::nullopt;
        }
};
}

static auto generate() {
	using namespace cobra;

        std::ifstream stream("config/simple_conf.cobra");

	config::basic_diagnostic_reporter reporter(false);
	config::parse_session session(stream, reporter);

        std::vector<config::server_config> configs = config::server_config::parse_servers(session);
        std::vector<std::shared_ptr<config::server>> servers;
        servers.reserve(configs.size());

        for (auto&& config : configs) {
                servers.push_back(std::make_shared<config::server>(config::server(std::move(config))));
        }

        sequential_executor exec;
        epoll_event_loop loop(exec);

        return server::convert(servers, &exec, &loop);
}

void test_one(const uint8_t *data, size_t size, cobra::server& server) {
	using namespace cobra;

        std::stringstream out;
        address addr("127.0.0.1:5423");
	fuzz_socket_stream stream(std::stringstream(std::string(reinterpret_cast<const char*>(data), size)), out, addr);

        block_task(server.on_connect(stream));

        auto cobra_stream = std_istream<std::stringstream>(std::move(out));
        auto cobra_stream_buf = istream_buffer(std::move(cobra_stream), 1024);

        http_response resp = block_task(parse_http_response(cobra_stream_buf));
        assert(resp.code() != 500);
}

#ifndef COBRA_FUZZ_AFLPP

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

        static auto servers = generate();
        test_one(data, size, servers.at(0));
        return 0;
}

#else
__AFL_FUZZ_INIT();

main() {

        auto servers = generate();

#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif

        unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

        while (__AFL_LOOP(10000)) {

                size_t len = __AFL_FUZZ_TESTCASE_LEN;

                if (len < 8) continue;
                test_one(buf, len, servers.at(0));
        }
        return 0;
}

#endif
#endif
