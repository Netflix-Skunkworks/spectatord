#include "logger.h"
#include <iostream>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace spectatord {

static constexpr const char* const kMainLogger = "spectatord";

LogManager& log_manager() noexcept {
  static auto* the_log_manager = new LogManager();
  return *the_log_manager;
}

LogManager::LogManager() noexcept {
  try {
    logger_ = spdlog::create_async_nb<spdlog::sinks::ansicolor_stdout_sink_mt>(
        kMainLogger);
  } catch (const spdlog::spdlog_ex& ex) {
    std::cerr << "Log initialization failed: " << ex.what() << "\n";
  }
}

std::shared_ptr<spdlog::logger> LogManager::Logger() noexcept {
  return logger_;
}

std::shared_ptr<spdlog::logger> LogManager::GetLogger(
    const std::string& name) noexcept {
  return spdlog::create_async_nb<spdlog::sinks::ansicolor_stdout_sink_mt>(name);
}

}  // namespace spectatord
