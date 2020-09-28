#include "monotonic_counter.h"
#include "common_refs.h"

namespace spectator {

static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();

MonotonicCounter::MonotonicCounter(Id id) noexcept
    : Meter{std::move(id)}, value_(kNaN), prev_value_(kNaN) {}

void MonotonicCounter::Set(double amount) noexcept {
  Update();
  value_.store(amount, std::memory_order_relaxed);
}

double MonotonicCounter::Delta() const noexcept {
  return value_.load(std::memory_order_relaxed) -
         prev_value_.load(std::memory_order_relaxed);
}

void MonotonicCounter::Measure(Measurements* results) const noexcept {
  auto delta = Delta();
  prev_value_.store(value_.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
  if (delta > 0) {
    if (!count_id_) {
      count_id_ = std::make_unique<Id>(MeterId().WithStat(refs().count()));
    }
    results->emplace_back(*count_id_, delta);
  }
}

}  // namespace spectator
