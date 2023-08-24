#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/executor.hh"
#include "cobra/asyncio/future_task.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/asyncio/std_stream.hh"
#include "cobra/compress/lz.hh"
#include "cobra/http/writer.hh"
#include "cobra/http/parse.hh"
#include "cobra/http/server.hh"
#include "cobra/net/stream.hh"
#include "cobra/process.hh"
#include "cobra/print.hh"
#include "cobra/config.hh"
#include "cobra/args.hh"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <limits>
#include <memory>

#include <cassert>
#include <optional>
#include <sstream>

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

struct args_type {
	std::string program_name;
	std::optional<std::string> config_file;
	std::optional<std::string> compress_file;
	std::optional<std::string> round_file;
	std::optional<std::string> deflate_file;
	std::optional<std::string> inflate_file;
	bool json = false;
	bool check = false;
	bool help = false;
};

#ifndef COBRA_FUZZ
int main(int argc, char **argv) {
	using namespace cobra;
	thread_pool_executor exec;
	//sequential_executor exec;
	platform_event_loop loop(exec);
	std::fstream file;
	std::istream* input = &std::cin;

	assert(signal(SIGPIPE, SIG_IGN) != SIG_ERR);

	auto parser = argument_parser<args_type>()
		.add_program_name(&args_type::program_name)
		.add_positional(&args_type::config_file, false, "file", "path to configuration file")
		.add_argument(&args_type::compress_file, "l", "ls", "test lempel-ziv")
		.add_argument(&args_type::round_file, "r", "round", "test lempel-ziv round trip")
		.add_argument(&args_type::deflate_file, "z", "deflate", "test deflate")
		.add_argument(&args_type::inflate_file, "d", "inflate", "test inflate")
		.add_flag(&args_type::json, true, "j", "json", "write diagnostics in json format")
		.add_flag(&args_type::check, true, "c", "check", "exit after reading configuration file")
		.add_flag(&args_type::help, true, "h", "help", "display this help message");
	auto args = parser.parse(argv, argv + argc);

	if (args.help) {
		parser.help();
		return EXIT_SUCCESS;
	}

	if (args.compress_file) {
		std::fstream f = std::fstream(*args.compress_file, std::ios::in);
		auto stream = istream_buffer<std_istream<std::fstream>>(std_istream<std::fstream>(std::move(f)), 1024);
		//unsigned short window_size = 1 << 8;
		uint16_t window_size = std::numeric_limits<uint16_t>::max();
		auto debug_stream = lz_debug_istream(window_size);
		auto lz_stream = lz_ostream<lz_debug_istream>(std::move(debug_stream), window_size);
		block_task(pipe(buffered_istream_reference(stream), ostream_reference(lz_stream)));

		return EXIT_SUCCESS;
	}

	if (args.round_file) {
		std::fstream f = std::fstream(*args.round_file, std::ios::in);
		auto stream = istream_buffer(std_istream<std::fstream>(std::move(f)), 1024);
		uint16_t window_size = std::numeric_limits<uint16_t>::max();
		//unsigned short window_size = 1 << 8;
		auto istream = lz_istream(window_size);
		auto ostream = lz_ostream<lz_istream>(std::move(istream), window_size);
		block_task(pipe(buffered_istream_reference(stream), ostream_reference(ostream)));
		auto out = std_ostream_reference(std::cout);
		auto new_stream = block_task(std::move(ostream).end());
		block_task(pipe(buffered_istream_reference(new_stream), ostream_reference(out)));

		return EXIT_SUCCESS;
	}

	if (args.deflate_file) {
		std::fstream f = std::fstream(*args.deflate_file, std::ios::in);
		istream_buffer defl_istream(std_istream(std::move(f)), 1024);
		deflate_ostream defl_ostream((std_ostream_reference(std::cout)));
		block_task(pipe(buffered_istream_reference(defl_istream), ostream_reference(defl_ostream)));
		block_task(std::move(defl_ostream).end());

		return EXIT_SUCCESS;
	}

	if (args.inflate_file) {
		std::fstream f = std::fstream(*args.inflate_file, std::ios::in);
		inflate_istream infl_istream(std_istream(std::move(f)));
		std_ostream_reference infl_ostream(std::cout);
		block_task(pipe(buffered_istream_reference(infl_istream), ostream_reference(infl_ostream)));

		return EXIT_SUCCESS;

	}

	if (args.config_file) {
		file = std::fstream(*args.config_file, std::ios::in);
		input = &file;
	}

	if (args.json) {
		config::basic_diagnostic_reporter reporter(false);
		config::parse_session session(*input, reporter);
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
		config::basic_diagnostic_reporter reporter(true);
		config::parse_session session(*input, reporter);

		try {
			std::vector<std::shared_ptr<config::server>> srvs;

			{
				std::vector<config::server_config> configs = config::server_config::parse_servers(session);

				eprintln("loaded {} server config(s)", configs.size());
				for (auto&& config : configs) {
					srvs.push_back(std::make_shared<config::server>(config::server(config)));
				}
			}

			for (auto& server : srvs) {
				server->debug_print(std::cerr, 0);
			}
			std::vector<server> servers = server::convert(srvs, &exec, &loop);
			eprintln("setup {} server(s)", servers.size());
			std::vector<future_task<void>> jobs;

			if (!args.check) {
				for (auto&& server : servers) {
					jobs.push_back(make_future_task(server.start(&exec, &loop)));
				}

				while (true) {
					loop.poll();
				}

				for (auto&& job : jobs) {
					job.get_future().get();
				}
			}
		} catch (const config::error& err) {
			session.report(err.diag());
		}
	}
	
	return EXIT_SUCCESS;
}
#endif

#endif
