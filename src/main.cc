#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/executor.hh"
#include "cobra/asyncio/future_task.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/asyncio/std_stream.hh"
#include "cobra/http/writer.hh"
#include "cobra/http/parse.hh"
#include "cobra/net/stream.hh"
#include "cobra/process.hh"
#include "cobra/print.hh"
#include "cobra/config.hh"
#include "cobra/http/server.hh"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <memory>

extern "C" {
#include <sys/socket.h>
}

#ifndef COBRA_TEST

static void print_json_diags(const std::vector<cobra::config::diagnostic>& diags) {
	using namespace cobra;
	const char* delim = "";
	print("[");

	for (const config::diagnostic& diag : diags) {
		print("{}{{", delim);
		print("\"start\":{{\"line\":{},\"col\":{}}},", diag.part.start.line, diag.part.start.col);
		print("\"end\":{{\"line\":{},\"col\":{}}},", diag.part.end.line, diag.part.end.col);
		print("\"message\":{},", cobra::quoted(diag.message));
		print("\"lvl\":{},", cobra::quoted(std::format("{}", diag.lvl)));
		print("\"primary_label\":{},", cobra::quoted(diag.primary_label));
		print("\"secondary_label\":{},", cobra::quoted(diag.secondary_label));
		print("\"sub_diags\":");
		print_json_diags(diag.sub_diags);
		print("}}");
		delim = ",";
	}

	print("]");
}

int main(int argc, char **argv) {
	using namespace cobra;
	sequential_executor exec;
	epoll_event_loop loop(exec);

	if (argc == 1) {
		config::basic_diagnostic_reporter reporter(false);
		config::parse_session session(std::cin, reporter);

		try {
			config::server_config::parse_servers(session);
		} catch (const config::error& err) {
			session.report(err.diag());
		}

		print_json_diags(reporter.get_diags());
	} else {
		std::fstream stream(argv[1], std::ios::in);
		config::basic_diagnostic_reporter reporter(true);
		config::parse_session session(stream, reporter);

		try {
			std::vector<std::shared_ptr<config::server>> srvs;

			{
				std::vector<config::server_config> configs = config::server_config::parse_servers(session);

				eprintln("loaded {} server config(s)", configs.size());
				for (auto&& config : configs) {
					srvs.push_back(std::make_shared<config::server>(config::server(config)));
				}
			}

			std::vector<server> servers = server::convert(srvs, &exec, &loop);
			eprintln("setup {} server(s)", servers.size());
			std::vector<future_task<void>> jobs;

			for (auto&& server : servers) {
				jobs.push_back(make_future_task(server.start(&exec, &loop)));
			}

			while (true) {
				loop.poll();
			}

			for (auto&& job : jobs) {
				job.get_future().wait();
			}
		} catch (const config::error& err) {
			session.report(err.diag());
		}
	}
	
	return EXIT_SUCCESS;
}

#endif
