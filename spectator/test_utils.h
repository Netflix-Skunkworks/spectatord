#pragma once

#include "../spectator/registry.h"
#include <map>
#include <string>
#include <vector>

std::map<std::string, double> measurements_to_map(
    const std::vector<spectator::Measurement>& measurements);

template <typename T>
std::vector<const T*> filter_my_meters(const std::vector<const T*> source) {
  std::vector<const T*> result;
  std::copy_if(source.begin(), source.end(), std::back_inserter(result),
               [](const T* t) {
                 std::string name = t->MeterId().Name().Get();
                 auto found = name.rfind("spectator.", 0);
                 return found == std::string::npos;
               });
  return result;
}

inline std::vector<const spectator::Timer*> my_timers(
    const spectator::Registry& registry) {
  using spectator::Timer;
  return filter_my_meters(registry.Timers());
}

inline std::vector<const spectator::Counter*> my_counters(
    const spectator::Registry& registry) {
  return filter_my_meters(registry.Counters());
}

inline std::vector<const spectator::DistributionSummary*> my_ds(
    const spectator::Registry& registry) {
  return filter_my_meters(registry.DistSummaries());
}

inline std::vector<const spectator::Gauge*> my_gauges(
    const spectator::Registry& registry) {
  return filter_my_meters(registry.Gauges());
}

inline std::vector<const spectator::MaxGauge*> my_max_gauges(
    const spectator::Registry& registry) {
  return filter_my_meters(registry.MaxGauges());
}

inline std::vector<const spectator::MonotonicCounter*> my_mono_counters(
    const spectator::Registry& registry) {
  return filter_my_meters(registry.MonotonicCounters());
}

inline size_t my_meters_size(const spectator::Registry& registry) {
  return my_timers(registry).size() + my_counters(registry).size() +
         my_gauges(registry).size() + my_max_gauges(registry).size() +
         my_ds(registry).size() + my_mono_counters(registry).size();
}
