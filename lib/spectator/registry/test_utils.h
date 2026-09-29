#pragma once

#include "registry.h"
#include <map>
#include <string>
#include <vector>

auto measurements_to_map(const std::vector<spectator::Measurement>& measurements) -> std::map<std::string, double>;

template <typename T>
auto filter_my_meters(const std::vector<const T*> source) -> std::vector<const T*>
{
	std::vector<const T*> result;
	std::copy_if(source.begin(), source.end(), std::back_inserter(result),
	             [](const T* t)
	             {
		             std::string name = t->MeterId().Name().Get();
		             auto found = name.rfind("spectator.", 0);
		             return found == std::string::npos;
	             });
	return result;
}

inline auto my_timers(const spectator::Registry& registry) -> std::vector<const spectator::Timer*>
{
	using spectator::Timer;
	return filter_my_meters(registry.Timers());
}

inline auto my_counters(const spectator::Registry& registry) -> std::vector<const spectator::Counter*>
{
	return filter_my_meters(registry.Counters());
}

inline auto my_ds(const spectator::Registry& registry) -> std::vector<const spectator::DistributionSummary*>
{
	return filter_my_meters(registry.DistSummaries());
}

inline auto my_gauges(const spectator::Registry& registry) -> std::vector<const spectator::Gauge*>
{
	return filter_my_meters(registry.Gauges());
}

inline auto my_age_gauges(const spectator::Registry& registry) -> std::vector<const spectator::AgeGauge*>
{
	return filter_my_meters(registry.AgeGauges());
}

inline auto my_max_gauges(const spectator::Registry& registry) -> std::vector<const spectator::MaxGauge*>
{
	return filter_my_meters(registry.MaxGauges());
}

inline auto my_mono_counters(const spectator::Registry& registry) -> std::vector<const spectator::MonotonicCounter*>
{
	return filter_my_meters(registry.MonotonicCounters());
}

inline auto my_meters_size(const spectator::Registry& registry) -> size_t
{
	return my_timers(registry).size() + my_counters(registry).size() + my_gauges(registry).size() +
	       my_age_gauges(registry).size() + my_max_gauges(registry).size() + my_ds(registry).size() +
	       my_mono_counters(registry).size();
}
