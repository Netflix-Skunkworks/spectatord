#pragma once

#include "meter.h"
#include <atomic>

namespace spectator {

class Counter : public Meter {
 public:
  explicit Counter(Id id) noexcept;
  void Measure(Measurements* results) const noexcept;
  void Increment() noexcept;
  void Add(double delta) noexcept;
  double Count() const noexcept;

 private:
  mutable std::unique_ptr<Id> count_id_;
  mutable std::atomic<double> count_;
};

}  // namespace spectator
