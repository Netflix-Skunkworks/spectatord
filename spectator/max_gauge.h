#pragma once

#include "meter.h"
#include <atomic>

namespace spectator
{

class MaxGauge : public Meter
{
   public:
	explicit MaxGauge(Id id) noexcept;
	void Measure(Measurements* results) const noexcept;
	void Update(double value) noexcept;

	// synonym for Update for consistency with the Gauge interface
	void Set(double value) noexcept { Update(value); }
	auto Get() const noexcept -> double;

   private:
	mutable std::unique_ptr<Id> max_id_;
	mutable std::atomic<double> value_;
};

}  // namespace spectator
