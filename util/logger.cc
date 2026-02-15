#include "logger.h"
#include "tcp_log_sink.h"
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace spectatord
{

Logger::Logger(const std::string& name, bool enable_insight_logs)
{
	logger_ = spdlog::get(name);
	if (logger_)
	{
		return;
	}

	if (!spdlog::thread_pool())
	{
		spdlog::init_thread_pool(8192, 1);
	}

	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>());
	if (enable_insight_logs)
	{
		sinks.push_back(create_insight_logs_sink("127.0.0.1", 1552));
	}

	logger_ = std::make_shared<spdlog::async_logger>(name, sinks.begin(), sinks.end(), spdlog::thread_pool(),
	                                                 spdlog::async_overflow_policy::overrun_oldest);
	spdlog::register_logger(logger_);
}

}  // namespace spectatord
