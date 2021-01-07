#include "timer.h"
#include "atomicnumber.h"
#include "common_refs.h"

namespace spectator {

Timer::Timer(Id id) noexcept
    : Meter(std::move(id)), count_(0), total_(0), totalSq_(0.0), max_(0) {}

void Timer::Measure(Measurements* results) const noexcept {
  auto cnt = count_.exchange(0, std::memory_order_relaxed);
  if (cnt == 0) {
    return;
  }

  if (!st) {
    st = std::make_unique<DistStats>(MeterId(), refs().totalTime());
  }

  auto total = total_.exchange(0, std::memory_order_relaxed);
  auto total_secs = total / 1e9;
  auto t_sq = totalSq_.exchange(0.0, std::memory_order_relaxed);
  auto t_sq_secs = t_sq / 1e18;
  auto mx = max_.exchange(0, std::memory_order_relaxed);
  auto mx_secs = mx / 1e9;
  results->emplace_back(st->count, static_cast<double>(cnt));
  results->emplace_back(st->total, total_secs);
  results->emplace_back(st->totalSq, t_sq_secs);
  results->emplace_back(st->max, mx_secs);
}

void Timer::Record(absl::Duration amount) noexcept {
  Update();
  int64_t ns = amount / absl::Nanoseconds(1);
  if (ns >= 0) {
    count_.fetch_add(1, std::memory_order_relaxed);
    total_.fetch_add(ns, std::memory_order_relaxed);
    add_double(&totalSq_, static_cast<double>(ns) * ns);
    update_max(&max_, ns);
  }
}

void Timer::Record(std::chrono::nanoseconds amount) noexcept {
  Record(absl::FromChrono(amount));
}

auto Timer::Count() const noexcept -> int64_t {
  return count_.load(std::memory_order_relaxed);
}

auto Timer::TotalTime() const noexcept -> int64_t {
  return total_.load(std::memory_order_relaxed);
}

}  // namespace spectator
