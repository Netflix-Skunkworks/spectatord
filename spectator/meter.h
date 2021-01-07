#pragma once

#include "absl/time/clock.h"
#include "id.h"
#include "measurement.h"
#include <atomic>
#include <chrono>
#include <vector>

namespace spectator {

struct Meter {
 public:
  explicit Meter(Id id)
      : id_{std::move(id)}, last_updated_{absl::GetCurrentTimeNanos()} {}
  [[nodiscard]] auto MeterId() const -> const Id& { return id_; }
  [[nodiscard]] auto Updated() const noexcept -> int64_t {
    return last_updated_;
  }

 private:
  Id id_;
  std::atomic_int64_t last_updated_;

 protected:
  auto Update() -> void { last_updated_ = absl::GetCurrentTimeNanos(); }
};

}  // namespace spectator
