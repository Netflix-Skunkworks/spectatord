#include "monotonic_sampled.h"

namespace spectator {

static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();
static constexpr auto kMax = std::numeric_limits<uint64_t>::max();

MonotonicSampled::MonotonicSampled(Id id) noexcept
    : Meter{std::move(id)},
      init_{false},
      value_{0},
      prev_value_{0},
      ts_{0},
      prev_ts_{0} {}

void MonotonicSampled::Set(uint64_t amount, uint64_t ts_nanos) noexcept {
  Update();
  absl::MutexLock lock(&mutex_);

  // ignore out-of-order points, overflows are not expected for timestamps
  if (ts_nanos < ts_) {
    return;
  }

  if (init_) {
    prev_value_ = value_;
    prev_ts_ = ts_;
  }

  value_ = amount;
  ts_ = ts_nanos;
}

void MonotonicSampled::Measure(Measurements* results) const noexcept {
  auto sampled_delta = SampledRate();

  {
    absl::MutexLock lock(&mutex_);
    prev_value_ = value_;
    prev_ts_ = ts_;
    init_ = true;
  }

  if (sampled_delta > 0) {
    if (!count_id_) {
      count_id_ = std::make_unique<Id>(MeterId().WithStat(refs().count()));
    }
    results->emplace_back(*count_id_, sampled_delta);
  }
}

auto MonotonicSampled::SampledRate() const noexcept -> double {
  absl::MutexLock lock(&mutex_);
  if (!init_) return kNaN;
  auto delta_t = (ts_ - prev_ts_) / 1e9;

  if (value_ < prev_value_) {
    return (kMax - prev_value_ + value_ + 1) / delta_t;
  } else {
    return (value_ - prev_value_) / delta_t;
  }
}

}  // namespace spectator