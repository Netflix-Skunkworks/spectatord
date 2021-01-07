#pragma once

#include "meter.h"
#include <atomic>

namespace spectator {

class Counter : public Meter {
 public:
  explicit Counter(Id id) noexcept;
  auto Measure(Measurements* results) const noexcept -> void;
  auto Increment() noexcept -> void;
  auto Add(double delta) noexcept -> void;
  [[nodiscard]] auto Count() const noexcept -> double;

 private:
  mutable std::unique_ptr<Id> count_id_;
  mutable std::atomic<double> count_;
};

}  // namespace spectator
