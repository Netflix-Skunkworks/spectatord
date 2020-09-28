#pragma once

#include <spdlog/spdlog.h>

namespace spectatord {

class LogManager {
 public:
  LogManager() noexcept;
  std::shared_ptr<spdlog::logger> Logger() noexcept;
  std::shared_ptr<spdlog::logger> GetLogger(const std::string& name) noexcept;

 private:
  std::shared_ptr<spdlog::logger> logger_;
};

LogManager& log_manager() noexcept;

inline std::shared_ptr<spdlog::logger> Logger() noexcept {
  return log_manager().Logger();
}
inline std::shared_ptr<spdlog::logger> GetLogger(
    const std::string& name) noexcept {
  return log_manager().GetLogger(name);
}

}  // namespace spectatord
