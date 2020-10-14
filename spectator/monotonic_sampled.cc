#include "monotonic_sampled.h"

namespace spectator {

static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();

MonotonicSampled::MonotonicSampled(Id id) noexcept
    : Meter{std::move(id)},
      value_{kNaN},
      prev_value_{kNaN},
      ts_{0},
      prev_ts_{0} {}

void MonotonicSampled::Set(double amount, int64_t ts_nanos) noexcept {
  Update();

  absl::MutexLock lock(&mutex_);
  // ignore out of order points, or wrap arounds
  if (ts_nanos < ts_) {
    return;
  }
  // only update prev values at most once per reporting interval
  if (std::isnan(prev_value_)) {
    prev_value_ = value_;
    prev_ts_ = ts_;
  }
  value_ = amount;
  ts_ = ts_nanos;
}

void MonotonicSampled::Measure(Measurements* results) const noexcept {
  auto sampled_delta = SampledRate();
  if (std::isnan(sampled_delta)) {
    return;
  }

  {
    absl::MutexLock lock(&mutex_);
    prev_value_ = value_;
    prev_ts_ = ts_;
  }

  if (sampled_delta > 0) {
    if (!count_id_) {
      count_id_ = std::make_unique<Id>(MeterId().WithStat(refs().count()));
    }
    results->emplace_back(*count_id_, sampled_delta);
  }
}

double MonotonicSampled::SampledRate() const noexcept {
  absl::MutexLock lock(&mutex_);
  auto delta_t = (ts_ - prev_ts_) / 1e9;
  return (value_ - prev_value_) / delta_t;
}

}  // namespace spectator