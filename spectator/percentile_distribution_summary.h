#pragma once

#include <algorithm>

#include "id.h"
#include "percentile_bucket_tags.inc"
#include "percentile_buckets.h"
#include "registry.h"

namespace spectator
{

class PercentileDistributionSummary
{
   public:
	PercentileDistributionSummary(Registry* registry, Id id, int64_t min, int64_t max) noexcept
	    : registry_{registry},
	      id_{std::move(id)},
	      min_{min},
	      max_{max},
	      dist_summary_{registry->GetDistributionSummary(id_)}
	{
	}

	auto get_counter(size_t index, const std::string* perc_tags) -> std::shared_ptr<Counter>
	{
		using spectator::refs;
		auto counterId =
		    id_.WithTags(refs().statistic(), refs().percentile(), refs().percentile(), intern_str(perc_tags[index]));
		return registry_->GetCounter(std::move(counterId));
	}

	void Record(int64_t amount) noexcept
	{
		if (amount < 0)
		{
			return;
		}

		dist_summary_->Record(amount);
		auto restricted = std::clamp(amount, min_, max_);
		auto index = PercentileBucketIndexOf(restricted);
		auto c = get_counter(index, kDistTags.begin());
		c->Increment();
	}

	auto MeterId() const noexcept -> const Id& { return id_; }
	auto Count() const noexcept -> int64_t { return dist_summary_->Count(); }
	auto TotalAmount() const noexcept -> double { return dist_summary_->TotalAmount(); }

   private:
	Registry* registry_;
	Id id_;
	int64_t min_;
	int64_t max_;
	std::shared_ptr<DistributionSummary> dist_summary_;
};

}  // namespace spectator
