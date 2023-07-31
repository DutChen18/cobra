#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/executor.hh"
#include "cobra/asyncio/future_task.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/asyncio/std_stream.hh"
#include "cobra/asyncio/deflate.hh"
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

#include <cassert>

extern "C" {
#include <sys/socket.h>
#include <openssl/ssl.h>
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

	assert(signal(SIGPIPE, SIG_IGN) != SIG_ERR);

	if (argc == 3) {
		std_istream_reference cin_istream(std::cin);
		std_ostream_reference cout_ostream(std::cout);
		inflate_istream istream(std::move(cin_istream));
		make_future_task(cobra::pipe(buffered_istream_reference(istream), ostream_reference(cout_ostream))).get_future().get();
	} else if (argc == 1) {
		config::basic_diagnostic_reporter reporter(false);
		config::parse_session session(std::cin, reporter);
		const char* delim = "";

		try {
			config::server_config::parse_servers(session);
		} catch (const config::error& err) {
			session.report(err.diag());
		}

		print("{{");
		print("\"diags\":");
		print_json_diags(reporter.get_diags());
		print(",");
		print("\"tokens\":");
		print("[");

		for (const auto& [part, type] : reporter.get_tokens()) {
			print("{}{{", delim);
			print("\"start\":{{\"line\":{},\"col\":{}}},", part.start.line, part.start.col);
			print("\"end\":{{\"line\":{},\"col\":{}}},", part.end.line, part.end.col);
			print("\"type\":{}", cobra::quoted(type));
			print("}}");
			delim = ",";
		}

		print("],");
		print("\"inlay_hints\":");
		print("[");
		delim = "";

		for (const auto& [pos, hint] : reporter.get_inlay_hints()) {
			print("{}{{", delim);
			print("\"line\":{},", pos.line);
			print("\"col\":{},", pos.col);
			print("\"hint\":{}", cobra::quoted(hint));
			print("}}");
			delim = ",";
		}

		print("]");
		print("}}");
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
