#pragma once

#include <chrono>
#include <string>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <spdlog/formatter.h>
#include <spdlog/sinks/tcp_sink.h>

namespace spectatord
{

class JsonLogFormatter : public spdlog::formatter
{
   public:
	void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override
	{
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time.time_since_epoch()).count();

		rapidjson::StringBuffer buf;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

		writer.StartObject();
		writer.Key("message");
		writer.String(msg.payload.data(), static_cast<rapidjson::SizeType>(msg.payload.size()));
		writer.Key("level");
		writer.String(level_to_string(msg.level));
		writer.Key("logts");
		writer.Int64(millis);
		writer.Key("logger");
		writer.String(msg.logger_name.data(), static_cast<rapidjson::SizeType>(msg.logger_name.size()));
		writer.EndObject();

		dest.append(buf.GetString(), buf.GetString() + buf.GetSize());
		dest.push_back('\n');
	}

	[[nodiscard]] auto clone() const -> std::unique_ptr<spdlog::formatter> override
	{
		return std::make_unique<JsonLogFormatter>();
	}

   private:
	static auto level_to_string(spdlog::level::level_enum level) -> const char*
	{
		switch (level)
		{
			case spdlog::level::trace:
				return "trace";
			case spdlog::level::debug:
				return "debug";
			case spdlog::level::info:
				return "info";
			case spdlog::level::warn:
				return "warn";
			case spdlog::level::err:
				return "error";
			case spdlog::level::critical:
				return "fatal";
			default:
				return "info";
		}
	}
};

inline auto create_insight_logs_sink(const std::string& host, int port) -> spdlog::sink_ptr
{
	spdlog::sinks::tcp_sink_config cfg{host, port};
	cfg.lazy_connect = true;

	auto sink = std::make_shared<spdlog::sinks::tcp_sink_mt>(cfg);
	sink->set_formatter(std::make_unique<JsonLogFormatter>());
	return sink;
}

}  // namespace spectatord
