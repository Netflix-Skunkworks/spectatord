#pragma once

#include "meter.h"
#include <atomic>

namespace spectator
{
class MonotonicCounterUint : public Meter
{
   public:
	explicit MonotonicCounterUint(Id id) noexcept;
	void Measure(Measurements* results) const noexcept;

	void Set(uint64_t amount) noexcept;
	auto Delta() const noexcept -> double;

   private:
	mutable std::unique_ptr<Id> count_id_;
	mutable std::atomic<bool> init_;
	mutable std::atomic<uint64_t> value_;
	mutable std::atomic<uint64_t> prev_value_;
};
}  // namespace spectator
