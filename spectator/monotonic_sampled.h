#pragma once

#include "meter.h"
#include "absl/synchronization/mutex.h"

namespace spectator {
class MonotonicSampled : public Meter {
 public:
  explicit MonotonicSampled(Id id) noexcept;
  void Measure(Measurements* results) const noexcept;

  void Set(double amount, int64_t ts_nanos) noexcept;
  auto SampledRate() const noexcept -> double;

 private:
  mutable std::unique_ptr<Id> count_id_;
  mutable absl::Mutex mutex_;
  double value_ GUARDED_BY(mutex_);
  mutable double prev_value_ GUARDED_BY(mutex_);
  int64_t ts_ GUARDED_BY(mutex_);
  mutable int64_t prev_ts_ GUARDED_BY(mutex_);
};
}  // namespace spectator
