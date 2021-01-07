#pragma once

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "config.h"
#include "counter.h"
#include "dist_summary.h"
#include "gauge.h"
#include "max_gauge.h"
#include "monotonic_counter.h"
#include "publisher.h"
#include "timer.h"
#include "spectator/monotonic_sampled.h"
#include <condition_variable>
#include <mutex>
#include <spdlog/spdlog.h>
#include <tsl/hopscotch_map.h>

namespace spectator {

namespace detail {

template <typename T>
inline auto is_meter_expired(int64_t now, const T& m, int64_t meter_ttl)
    -> bool {
  auto updated_ago = now - m.Updated();
  return updated_ago > meter_ttl;
}

template <>
inline auto is_meter_expired<Gauge>(int64_t now, const Gauge& m,
                                    int64_t /* meter_ttl */) -> bool {
  return m.HasExpired(now);
}

template <typename M>
struct meter_map {
  mutable absl::Mutex meters_mutex_{};
  using table_t = tsl::hopscotch_map<Id, std::shared_ptr<M>>;
  table_t meters_ GUARDED_BY(meters_mutex_);

  auto size() const noexcept {
    absl::MutexLock lock(&meters_mutex_);
    return meters_.size();
  }

  // only insert if it doesn't exist, otherwise return the existing meter
  auto insert(std::shared_ptr<M> meter) -> std::shared_ptr<M> {
    absl::MutexLock lock(&meters_mutex_);
    const auto& id = meter->MeterId();
    auto insert_result = meters_.emplace(id, std::move(meter));
    return insert_result.first->second;
  }

  void measure(std::vector<Measurement>* res, int64_t meter_ttl) const {
    absl::MutexLock lock(&meters_mutex_);
    auto now = absl::GetCurrentTimeNanos();

    for (const auto& pair : meters_) {
      const auto& m = pair.second;
      if (!is_meter_expired(now, *m, meter_ttl)) {
        m->Measure(res);
      }
    }
  }

  auto remove_expired(int64_t meter_ttl) noexcept -> std::pair<int, int> {
    auto now = absl::GetCurrentTimeNanos();
    auto expired = 0;
    auto total = 0;

    {
      absl::MutexLock lock{&meters_mutex_};
      auto it = meters_.begin();
      auto end = meters_.end();
      while (it != end) {
        ++total;
        if (is_meter_expired(now, *it->second, meter_ttl)) {
          it = meters_.erase(it);
          ++expired;
        } else {
          ++it;
        }
      }
    }
    return {expired, total};
  }

  auto get_values() const -> std::vector<const M*> {
    std::vector<const M*> res;
    {
      absl::MutexLock lock{&meters_mutex_};
      res.reserve(meters_.size());
      for (const auto& pair : meters_) {
        res.emplace_back(pair.second.get());
      }
    }
    return res;
  }
};

struct all_meters {
  meter_map<Counter> counters_;
  meter_map<DistributionSummary> dist_sums_;
  meter_map<Gauge> gauges_;
  meter_map<MaxGauge> max_gauges_;
  meter_map<MonotonicCounter> mono_counters_;
  meter_map<MonotonicSampled> mono_sampled_;
  meter_map<Timer> timers_;

  auto size() const -> size_t {
    return counters_.size() + dist_sums_.size() + gauges_.size() +
           max_gauges_.size() + mono_counters_.size() + timers_.size();
  }

  auto measure(int64_t meter_ttl) const -> std::vector<Measurement> {
    std::vector<Measurement> res;
    res.reserve(size() * 2);
    counters_.measure(&res, meter_ttl);
    dist_sums_.measure(&res, meter_ttl);
    gauges_.measure(&res, meter_ttl);
    max_gauges_.measure(&res, meter_ttl);
    mono_counters_.measure(&res, meter_ttl);
    timers_.measure(&res, meter_ttl);
    return res;
  }

  auto remove_expired(int64_t meter_ttl) -> std::pair<int, int> {
    int total_expired = 0;
    int total_count = 0;
    int expired = 0;
    int count = 0;
    std::tie(expired, count) = counters_.remove_expired(meter_ttl);
    total_expired += expired;
    total_count += count;
    std::tie(expired, count) = dist_sums_.remove_expired(meter_ttl);
    total_expired += expired;
    total_count += count;
    std::tie(expired, count) = gauges_.remove_expired(meter_ttl);
    total_expired += expired;
    total_count += count;
    std::tie(expired, count) = max_gauges_.remove_expired(meter_ttl);
    total_expired += expired;
    total_count += count;
    std::tie(expired, count) = mono_counters_.remove_expired(meter_ttl);
    total_expired += expired;
    total_count += count;
    std::tie(expired, count) = timers_.remove_expired(meter_ttl);
    total_expired += expired;
    total_count += count;
    return {total_expired, total_count};
  }

  auto insert_counter(Id id) {
    return counters_.insert(std::make_shared<Counter>(std::move(id)));
  }

  auto insert_gauge(Id id, absl::Duration ttl) {
    return gauges_.insert(std::make_shared<Gauge>(std::move(id), ttl));
  }

  auto insert_max_gauge(Id id) {
    return max_gauges_.insert(std::make_shared<MaxGauge>(std::move(id)));
  }

  auto insert_mono_counter(Id id) {
    return mono_counters_.insert(
        std::make_shared<MonotonicCounter>(std::move(id)));
  }

  auto insert_mono_sampled(Id id) {
    return mono_sampled_.insert(
        std::make_shared<MonotonicSampled>(std::move(id)));
  }

  auto insert_dist_sum(Id id) {
    return dist_sums_.insert(
        std::make_shared<DistributionSummary>(std::move(id)));
  }

  auto insert_timer(Id id) {
    return timers_.insert(std::make_shared<Timer>(std::move(id)));
  }
};

}  // namespace detail

class Registry {
 public:
  using logger_ptr = std::shared_ptr<spdlog::logger>;
  using measurements_callback =
      std::function<void(const std::vector<Measurement>&)>;

  Registry(std::unique_ptr<Config> config, logger_ptr logger) noexcept;
  Registry(const Registry&) = delete;
  Registry(Registry&&) = delete;
  auto operator=(const Registry&) -> Registry& = delete;
  auto operator=(Registry &&) -> Registry& = delete;
  ~Registry() noexcept { Stop(); }
  auto GetConfig() const noexcept -> const Config&;
  auto GetLogger() const noexcept -> logger_ptr;

  void OnMeasurements(measurements_callback fn) noexcept;

  auto GetCounter(Id id) noexcept -> std::shared_ptr<Counter>;
  auto GetCounter(std::string_view name, Tags tags = {}) noexcept
      -> std::shared_ptr<Counter>;

  auto GetMonotonicCounter(Id id) noexcept -> std::shared_ptr<MonotonicCounter>;
  auto GetMonotonicCounter(std::string_view name, Tags tags = {}) noexcept
      -> std::shared_ptr<MonotonicCounter>;

  auto GetMonotonicSampled(Id id) noexcept -> std::shared_ptr<MonotonicSampled>;
  auto GetMonotonicSampled(std::string_view name, Tags tags = {}) noexcept
      -> std::shared_ptr<MonotonicSampled>;

  auto GetDistributionSummary(Id id) noexcept
      -> std::shared_ptr<DistributionSummary>;
  auto GetDistributionSummary(std::string_view name, Tags tags = {}) noexcept
      -> std::shared_ptr<DistributionSummary>;

  auto GetGauge(Id id) noexcept -> std::shared_ptr<Gauge>;
  auto GetGauge(Id id, absl::Duration ttl) noexcept -> std::shared_ptr<Gauge>;
  auto GetGauge(std::string_view name, Tags tags = {}) noexcept
      -> std::shared_ptr<Gauge>;

  auto GetMaxGauge(Id id) noexcept -> std::shared_ptr<MaxGauge>;
  auto GetMaxGauge(std::string_view name, Tags tags = {}) noexcept
      -> std::shared_ptr<MaxGauge>;

  auto GetTimer(Id id) noexcept -> std::shared_ptr<Timer>;
  auto GetTimer(std::string_view name, Tags tags = {}) noexcept
      -> std::shared_ptr<Timer>;

  auto Measurements() const noexcept -> std::vector<Measurement>;

  auto Size() const noexcept -> std::size_t { return all_meters_.size(); }

  void Start() noexcept;
  void Stop() noexcept;

  // for debugging / testing
  auto Timers() const -> std::vector<const Timer*> {
    return all_meters_.timers_.get_values();
  }
  auto Counters() const -> std::vector<const Counter*> {
    return all_meters_.counters_.get_values();
  }
  auto MonotonicCounters() const -> std::vector<const MonotonicCounter*> {
    return all_meters_.mono_counters_.get_values();
  }
  auto Gauges() const -> std::vector<const Gauge*> {
    return all_meters_.gauges_.get_values();
  }
  auto MaxGauges() const -> std::vector<const MaxGauge*> {
    return all_meters_.max_gauges_.get_values();
  }
  auto DistSummaries() const -> std::vector<const DistributionSummary*> {
    return all_meters_.dist_sums_.get_values();
  }

 private:
  std::atomic<bool> should_stop_;
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  std::thread expirer_thread_;
  int64_t meter_ttl_;  // in nanos

  std::unique_ptr<Config> config_;
  logger_ptr logger_;

  detail::all_meters all_meters_;

  std::vector<measurements_callback> ms_callbacks_{};
  std::shared_ptr<DistributionSummary> registry_size_;

  void expirer() noexcept;

  Publisher<Registry> publisher_;

 protected:
  // for testing
  void remove_expired_meters() noexcept;
};

}  // namespace spectator
