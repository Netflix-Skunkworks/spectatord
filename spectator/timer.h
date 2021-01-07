#pragma once
#include "absl/time/time.h"
#include "dist_stats.h"
#include "meter.h"
#include <atomic>
#include <chrono>

namespace spectator {

class Timer : public Meter {
 public:
  explicit Timer(Id id) noexcept;
  void Measure(Measurements* result) const noexcept;

  void Record(std::chrono::nanoseconds amount) noexcept;
  void Record(absl::Duration amount) noexcept;
  auto Count() const noexcept -> int64_t;
  auto TotalTime() const noexcept -> int64_t;

 private:
  mutable std::unique_ptr<DistStats> st;
  mutable std::atomic<int64_t> count_;
  mutable std::atomic<int64_t> total_;
  mutable std::atomic<double> totalSq_;
  mutable std::atomic<int64_t> max_;
};

}  // namespace spectator
