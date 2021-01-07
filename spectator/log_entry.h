#pragma once

#include "registry.h"
#include "strings.h"
#include "percentile_timer.h"
#include "absl/time/clock.h"

namespace spectator {
class LogEntry {
 public:
  LogEntry(Registry* registry, const std::string& method,
           const std::string& url)
      : registry_{registry},
        start_{absl::Now()},
        id_{Id::Of("ipc.client.call", {{"owner", "spectator-cpp"},
                                       {"ipc.endpoint", PathFromUrl(url)},
                                       {"http.method", method},
                                       {"http.status", "-1"}})} {}

  [[nodiscard]] auto start() const -> absl::Time { return start_; }

  void log() {
    PercentileTimer timer{registry_, std::move(id_), absl::Milliseconds(1),
                          absl::Seconds(10)};
    timer.Record(absl::Now() - start_);
  }

  void set_status_code(int code) {
    id_ = id_.WithTag(intern_str("http.status"),
                      intern_str(fmt::format("{}", code)));
  }

  void set_attempt(int attempt_number, bool is_final) {
    id_ = id_.WithTags(intern_str("ipc.attempt"), attempt(attempt_number),
                       intern_str("ipc.attempt.final"),
                       is_final ? intern_str("true") : intern_str("false"));
  }

  void set_error(const std::string& error) {
    id_ = id_.WithTags(intern_str("ipc.result"), intern_str("failure"),
                       intern_str("ipc.status"), intern_str(error));
  }

  void set_success() {
    static auto ipc_success = intern_str("success");
    id_ = id_.WithTags(intern_str("ipc.status"), ipc_success,
                       intern_str("ipc.result"), ipc_success);
  }

 private:
  Registry* registry_;
  absl::Time start_;
  Id id_;

  static auto attempt(int attempt_number) -> StrRef {
    static auto initial = intern_str("initial");
    static auto second = intern_str("second");
    static auto third_up = intern_str("third_up");

    switch (attempt_number) {
      case 0:
        return initial;
      case 1:
        return second;
      default:
        return third_up;
    }
  }
};

}  // namespace spectator
