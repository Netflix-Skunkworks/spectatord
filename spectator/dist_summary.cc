#include "dist_summary.h"
#include "atomicnumber.h"
#include "common_refs.h"

namespace spectator
{

DistributionSummary::DistributionSummary(Id id) noexcept
    : Meter(std::move(id)), count_(0), total_(0), totalSq_(0.0), max_(0)
{
}

auto DistributionSummary::Measure(Measurements* results) const noexcept -> void
{
	auto cnt = count_.exchange(0, std::memory_order_relaxed);
	if (cnt == 0)
	{
		return;
	}

	if (!st)
	{
		st = std::make_unique<DistStats>(MeterId(), refs().totalAmount());
	}

	auto total = total_.exchange(0.0, std::memory_order_relaxed);
	auto t_sq = totalSq_.exchange(0.0, std::memory_order_relaxed);
	auto mx = max_.exchange(0.0, std::memory_order_relaxed);
	results->emplace_back(st->count, static_cast<double>(cnt));
	results->emplace_back(st->total, total);
	results->emplace_back(st->totalSq, t_sq);
	results->emplace_back(st->max, mx);
}

auto DistributionSummary::Record(double amount) noexcept -> void
{
	Update();

	if (amount >= 0)
	{
		count_.fetch_add(1, std::memory_order_relaxed);
		add_double(&total_, amount);
		add_double(&totalSq_, amount * amount);
		update_max(&max_, amount);
	}
}

auto DistributionSummary::Count() const noexcept -> int64_t { return count_.load(std::memory_order_relaxed); }

auto DistributionSummary::TotalAmount() const noexcept -> double { return total_.load(std::memory_order_relaxed); }

}  // namespace spectator
