#include <cstdint>
#include <memory>
#include <string>
#include <sstream>
#include "cobra/config.hh"
#include "cobra/http/server.hh"
#include <cstddef>

#ifdef COBRA_FUZZ_CONFIG

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::stringstream stream(std::string(reinterpret_cast<const char*>(data), size));
	using namespace cobra;

	config::basic_diagnostic_reporter reporter(false);
	config::parse_session session(stream, reporter);

	try {
		std::vector<config::server_config> configs = config::server_config::parse_servers(session);
		std::vector<std::shared_ptr<config::server>> servers;
		servers.reserve(configs.size());

		for (auto&& config : configs) {
			servers.push_back(std::make_shared<config::server>(config::server(std::move(config))));
		}

		sequential_executor exec;
		epoll_event_loop loop(exec);

		/*(void)server::convert(servers, &exec, &loop);*/

	} catch (const config::error& err) {
	}
	return 0;
}
#endif
