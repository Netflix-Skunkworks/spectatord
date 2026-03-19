#pragma once

#include <string>

#include <spdlog/sinks/tcp_sink.h>

#include "json_log_formatter.h"
#include "unix_socket_log_sink.h"

namespace spectatord
{

inline auto create_insight_logs_sink(const std::string& host, int port) -> spdlog::sink_ptr
{
	spdlog::sinks::tcp_sink_config cfg{host, port};
	cfg.lazy_connect = true;

	auto sink = std::make_shared<spdlog::sinks::tcp_sink_mt>(cfg);
	sink->set_formatter(std::make_unique<JsonLogFormatter>());
	return sink;
}

inline auto create_insight_logs_unix_sink(const std::string& socket_path) -> spdlog::sink_ptr
{
	spdlog::sinks::unix_sink_config cfg{socket_path};

	auto sink = std::make_shared<spdlog::sinks::unix_sink_mt>(cfg);
	sink->set_formatter(std::make_unique<JsonLogFormatter>());
	return sink;
}

}  // namespace spectatord
