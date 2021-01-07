#pragma once
#include "dist_stats.h"
#include "meter.h"
#include <atomic>

namespace spectator {

class DistributionSummary : public Meter {
 public:
  explicit DistributionSummary(Id id) noexcept;
  auto Measure(Measurements* results) const noexcept -> void;
  auto Record(double amount) noexcept -> void;
  [[nodiscard]] auto Count() const noexcept -> int64_t;
  [[nodiscard]] auto TotalAmount() const noexcept -> double;

 private:
  mutable std::unique_ptr<DistStats> st;
  mutable std::atomic<int64_t> count_;
  mutable std::atomic<double> total_;
  mutable std::atomic<double> totalSq_;
  mutable std::atomic<double> max_;
};

}  // namespace spectator
