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
  int64_t Count() const noexcept;
  int64_t TotalTime() const noexcept;

 private:
  mutable std::unique_ptr<DistStats> st;
  mutable std::atomic<int64_t> count_;
  mutable std::atomic<int64_t> total_;
  mutable std::atomic<double> totalSq_;
  mutable std::atomic<int64_t> max_;
};

}  // namespace spectator
