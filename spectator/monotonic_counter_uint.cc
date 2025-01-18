#include "monotonic_counter_uint.h"
#include "common_refs.h"

namespace spectator {

static constexpr auto kOverflow = 9.223372e+18;  // 2^63
static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();
static constexpr auto kMax = std::numeric_limits<uint64_t>::max();

MonotonicCounterUint::MonotonicCounterUint(Id id) noexcept
    : Meter{std::move(id)}, init_(false), value_(0), prev_value_(0) {}

void MonotonicCounterUint::Set(uint64_t amount) noexcept {
  Update();
  value_.store(amount, std::memory_order_relaxed);
}

auto MonotonicCounterUint::Delta() const noexcept -> double {
  if (!init_.load(std::memory_order_relaxed)) return kNaN;

  auto prev = prev_value_.load(std::memory_order_relaxed);
  auto curr = value_.load(std::memory_order_relaxed);

  return ((curr < prev) == true) ? kMax - prev + curr + 1 : curr - prev;
}

void MonotonicCounterUint::Measure(Measurements* results) const noexcept {
  auto delta = Delta();
  prev_value_.store(value_.load(std::memory_order_relaxed), std::memory_order_relaxed);
  init_.store(true, std::memory_order_relaxed);

  if (delta > 0) {
    if (!count_id_) {
      count_id_ = std::make_unique<Id>(MeterId().WithDefaultStat(refs().count()));
    }
    if (delta > kOverflow) {
      // deltas that appear to be unexpected overflows will be reported as zeroes instead
      results->emplace_back(*count_id_, 0);
    } else {
      results->emplace_back(*count_id_, delta);
    }
  }
}

}  // namespace spectator
