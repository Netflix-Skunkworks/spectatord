#include "counter.h"
#include "atomicnumber.h"
#include "common_refs.h"

namespace spectator {

Counter::Counter(Id id) noexcept : Meter{std::move(id)}, count_{0.0} {}

void Counter::Measure(Measurements* results) const noexcept {
  auto count = count_.exchange(0.0, std::memory_order_relaxed);
  if (count > 0) {
    if (!count_id_) {
      count_id_ =
          std::make_unique<Id>(MeterId().WithDefaultStat(refs().count()));
    }
    results->emplace_back(*count_id_, count);
  }
}

void Counter::Increment() noexcept { Add(1.0); }

void Counter::Add(double delta) noexcept {
  Update();

  if (delta < 0) {
    return;
  }
  add_double(&count_, delta);
}

auto Counter::Count() const noexcept -> double {
  return count_.load(std::memory_order_relaxed);
}

}  // namespace spectator
