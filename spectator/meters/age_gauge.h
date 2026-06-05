#pragma once

#include "meter.h"
#include <atomic>

namespace spectator
{

class AgeGauge : public Meter
{
   public:
	explicit AgeGauge(Id id) noexcept : Meter{std::move(id)}, last_success_{0} {}

	void Measure(Measurements* results) const noexcept
	{
		if (!gauge_id_)
		{
			gauge_id_ = std::make_unique<Id>(MeterId().WithDefaultStat(refs().gauge()));
		}
		results->emplace_back(*gauge_id_, Value());
	}

	void UpdateLastSuccess(int64_t now = absl::GetCurrentTimeNanos()) noexcept
	{
		last_success_.store(now, std::memory_order_relaxed);
	}

	auto GetLastSuccess() const noexcept -> int64_t { return last_success_.load(std::memory_order_relaxed); }

	auto Value(int64_t now = absl::GetCurrentTimeNanos()) const noexcept -> double
	{
		auto last_success = GetLastSuccess();
		return static_cast<double>(now - last_success) / 1e9;
	}

   private:
	std::atomic<int64_t> last_success_;
	mutable std::unique_ptr<Id> gauge_id_;
};

}  // namespace spectator
