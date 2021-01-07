#include "max_gauge.h"
#include "atomicnumber.h"
#include "common_refs.h"

namespace spectator {

static constexpr auto kMinValue = std::numeric_limits<double>::lowest();
static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();

MaxGauge::MaxGauge(Id id) noexcept : Meter{std::move(id)}, value_{kMinValue} {}

void MaxGauge::Measure(Measurements* results) const noexcept {
  auto value = value_.exchange(kMinValue, std::memory_order_relaxed);
  if (value == kMinValue) {
    return;
  }
  if (!max_id_) {
    max_id_ = std::make_unique<Id>(MeterId().WithStat(refs().max()));
  }
  results->emplace_back(*max_id_, value);
}

auto MaxGauge::Get() const noexcept -> double {
  auto v = value_.load(std::memory_order_relaxed);
  if (v != kMinValue) {
    return v;
  }
  return kNaN;
}

void MaxGauge::Update(double value) noexcept {
  Meter::Update();
  update_max(&value_, value);
}

}  // namespace spectator
