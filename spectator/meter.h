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
  [[nodiscard]] const Id& MeterId() const { return id_; }
  [[nodiscard]] int64_t Updated() const noexcept { return last_updated_; }

 private:
  Id id_;
  std::atomic_int64_t last_updated_;

 protected:
  void Update() { last_updated_ = absl::GetCurrentTimeNanos(); }
};

}  // namespace spectator
