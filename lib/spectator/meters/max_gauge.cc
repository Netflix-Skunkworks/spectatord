#include "max_gauge.h"
#include "atomicnumber.h"
#include "spectator/strings/common_refs.h"

namespace spectator
{

static constexpr auto kMinValue = std::numeric_limits<double>::lowest();
static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();

MaxGauge::MaxGauge(Id id) noexcept : Meter{std::move(id)}, value_{kMinValue} {}

void MaxGauge::Measure(Measurements* results) const noexcept
{
	auto value = value_.exchange(kMinValue, std::memory_order_relaxed);
	if (value == kMinValue)
	{
		return;
	}
	if (!max_id_)
	{
		// Preserve an existing statistic tag (e.g. statistic=distinct for the registers of a
		// distinct count sketch); only default it to max when none is set. This matches the
		// way Counter uses WithDefaultStat and keeps the published statistic correct for
		// callers that decompose a metric into max gauges with their own statistic.
		max_id_ = std::make_unique<Id>(MeterId().WithDefaultStat(refs().max()));
	}
	results->emplace_back(*max_id_, value);
}

auto MaxGauge::Get() const noexcept -> double
{
	auto v = value_.load(std::memory_order_relaxed);
	if (v != kMinValue)
	{
		return v;
	}
	return kNaN;
}

void MaxGauge::Update(double value) noexcept
{
	Meter::Update();
	update_max(&value_, value);
}

}  // namespace spectator
