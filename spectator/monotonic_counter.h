#pragma once

#include "meter.h"
#include <atomic>

namespace spectator {
class MonotonicCounter : public Meter {
 public:
  explicit MonotonicCounter(Id id) noexcept;
  void Measure(Measurements* results) const noexcept;

  void Set(double amount) noexcept;
  auto Delta() const noexcept -> double;

 private:
  mutable std::unique_ptr<Id> count_id_;
  mutable std::atomic<double> value_;
  mutable std::atomic<double> prev_value_;
};
}  // namespace spectator
