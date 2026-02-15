#pragma once

#include <algorithm>

#include "id.h"
#include "percentile_bucket_tags.inc"
#include "percentile_buckets.h"
#include "registry.h"

namespace spectator
{

class PercentileTimer
{
   public:
	PercentileTimer(Registry* registry, Id id, absl::Duration min, absl::Duration max) noexcept
	    : registry_{registry}, id_{std::move(id)}, min_{min}, max_{max}, timer_{registry->GetTimer(id_)}
	{
	}

	auto get_counter(size_t index, const std::string* perc_tags) -> std::shared_ptr<Counter>
	{
		using spectator::refs;
		auto counterId =
		    id_.WithTags(refs().statistic(), refs().percentile(), refs().percentile(), intern_str(perc_tags[index]));
		return registry_->GetCounter(std::move(counterId));
	}

	void Record(absl::Duration amount) noexcept
	{
		timer_->Record(amount);
		auto restricted = std::clamp(amount, min_, max_);
		auto index = PercentileBucketIndexOf(absl::ToInt64Nanoseconds(restricted));
		auto c = get_counter(index, kTimerTags.begin());
		c->Increment();
	}

	void Record(std::chrono::nanoseconds amount) noexcept { Record(absl::FromChrono(amount)); }
	auto MeterId() const noexcept -> const Id& { return id_; }
	auto Count() const noexcept -> int64_t { return timer_->Count(); }
	auto TotalTime() const noexcept -> int64_t { return timer_->TotalTime(); }

   private:
	Registry* registry_;
	Id id_;
	absl::Duration min_;
	absl::Duration max_;
	std::shared_ptr<Timer> timer_;
};

}  // namespace spectator
