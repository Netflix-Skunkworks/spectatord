#include "logger.h"
#include "tcp_log_sink.h"
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sys/stat.h>

namespace spectatord
{

constexpr const char* kInsightLogsSocketPath = "/run/nflx-otel-collector/spectatord.sock";

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

	std::string insight_logs_dest;
	if (enable_insight_logs)
	{
		struct stat sb;
		if (stat(kInsightLogsSocketPath, &sb) == 0 && (sb.st_mode & S_IFSOCK) != 0)
		{
			sinks.push_back(create_insight_logs_unix_sink(kInsightLogsSocketPath));
			insight_logs_dest = kInsightLogsSocketPath;
		}
		else
		{
			sinks.push_back(create_insight_logs_sink("127.0.0.1", 1552));
			insight_logs_dest = "127.0.0.1:1552";
		}
	}

	logger_ = std::make_shared<spdlog::async_logger>(name, sinks.begin(), sinks.end(), spdlog::thread_pool(),
	                                                 spdlog::async_overflow_policy::overrun_oldest);
	spdlog::register_logger(logger_);

	if (!insight_logs_dest.empty())
	{
		logger_->info("insight logs enabled via {}", insight_logs_dest);
	}
	else
	{
		logger_->info("insight logs disabled");
	}
}

}  // namespace spectatord
