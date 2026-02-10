#pragma once

#include <string>
#include <spdlog/spdlog.h>

namespace spectatord {

class Logger {
 public:
  explicit Logger(const std::string& name = "spectatord", bool enable_insight_logs = false);

  auto operator->() const -> spdlog::logger* { return logger_.get(); }
  operator std::shared_ptr<spdlog::logger>() const { return logger_; }

 private:
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace spectatord
