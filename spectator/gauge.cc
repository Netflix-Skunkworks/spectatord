#include "gauge.h"
#include "common_refs.h"
#include <cmath>

namespace spectator {

static constexpr auto kNAN = std::numeric_limits<double>::quiet_NaN();

static int64_t get_ttl(absl::Duration ttl) {
  static constexpr auto kMinTtl = absl::Seconds(5);
  static constexpr int64_t kMinTtlNanos = int64_t(5) * 1000 * 1000 * 1000;
  if (ttl < kMinTtl) {
    return kMinTtlNanos;
  }
  return static_cast<int64_t>(absl::ToInt64Nanoseconds(ttl));
}

Gauge::Gauge(Id id, absl::Duration ttl) noexcept
    : Meter{std::move(id)}, ttl_nanos_{get_ttl(ttl)}, value_{kNAN} {}

bool Gauge::HasExpired(int64_t now) const noexcept {
  auto ago = now - Updated();
  return ago > ttl_nanos_;
}

void Gauge::Measure(Measurements* results, int64_t now) const noexcept {
  double value;
  if (HasExpired(now)) {
    value = value_.exchange(kNAN, std::memory_order_relaxed);
  } else {
    value = value_.load(std::memory_order_relaxed);
  }

  if (std::isnan(value)) {
    return;
  }
  if (!gauge_id_) {
    gauge_id_ = std::make_unique<Id>(MeterId().WithDefaultStat(refs().gauge()));
  }
  results->emplace_back(*gauge_id_, value);
}

void Gauge::Set(double value) noexcept {
  Update();

  value_.store(value, std::memory_order_relaxed);
}

double Gauge::Get() const noexcept {
  return value_.load(std::memory_order_relaxed);
}

void Gauge::SetTtl(absl::Duration ttl) noexcept {
  this->ttl_nanos_ = get_ttl(ttl);
}

}  // namespace spectator
