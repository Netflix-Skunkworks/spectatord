#include "registry.h"
#include "util/logger.h"
#include <fmt/ostream.h>

#include <utility>

namespace spectator {

Registry::Registry(std::unique_ptr<Config> config,
                   Registry::logger_ptr logger) noexcept
    : should_stop_{true},
      age_gauge_first_warn_{true},
      meter_ttl_{absl::ToInt64Nanoseconds(config->meter_ttl)},
      config_{std::move(config)},
      logger_{std::move(logger)},
      registry_size_{GetDistributionSummary("spectator.registrySize", Tags{{"owner", "spectatord"}})},
      publisher_(this) {
  if (meter_ttl_ == 0) {
    meter_ttl_ = int64_t(15) * 60 * 1000 * 1000 * 1000;
  }
}

auto Registry::GetConfig() const noexcept -> const Config& { return *config_; }

auto Registry::GetLogger() const noexcept -> Registry::logger_ptr {
  return logger_;
}

auto Registry::GetAgeGauge(Id id) noexcept -> std::shared_ptr<AgeGauge> {
  if (all_meters_.age_gauges_.contains(id)) {
    return all_meters_.age_gauges_.at(id);
  } else {
    if (all_meters_.age_gauges_.size() < config_->age_gauge_limit) {
      return all_meters_.insert_age_gauge(std::move(id));
    } else {
      logger_->warn("max number of age gauges ({}) has been reached, skipping creation",
                    config_->age_gauge_limit);

      if (age_gauge_first_warn_) {
        auto known_age_gauges = all_meters_.age_gauges_.get_ids();
        for (size_t i = 0; i < known_age_gauges.size(); ++i) {
          logger_->warn("known age gauge {}: {}", i, known_age_gauges[i]);
        }
        age_gauge_first_warn_ = false;
      }

      return std::make_shared<AgeGauge>(std::move(id));
    }
  }
}

auto Registry::GetAgeGauge(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<AgeGauge> {
  return GetAgeGauge(Id::Of(name, std::move(tags)));
}

auto Registry::GetCounter(Id id) noexcept -> std::shared_ptr<Counter> {
  return all_meters_.insert_counter(std::move(id));
}

auto Registry::GetCounter(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<Counter> {
  return GetCounter(Id::Of(name, std::move(tags)));
}

auto Registry::GetDistributionSummary(Id id) noexcept
    -> std::shared_ptr<DistributionSummary> {
  return all_meters_.insert_dist_sum(std::move(id));
}

auto Registry::GetDistributionSummary(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<DistributionSummary> {
  return GetDistributionSummary(Id::Of(name, std::move(tags)));
}

auto Registry::GetGauge(Id id) noexcept -> std::shared_ptr<Gauge> {
  return all_meters_.insert_gauge(std::move(id), GetConfig().meter_ttl);
}

auto Registry::GetGauge(Id id, absl::Duration ttl) noexcept
    -> std::shared_ptr<Gauge> {
  auto g = all_meters_.insert_gauge(std::move(id), ttl);
  g->SetTtl(ttl);  // in case the previous ttl was different
  return g;
}

auto Registry::GetGauge(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<Gauge> {
  return GetGauge(Id::Of(name, std::move(tags)));
}

auto Registry::GetMaxGauge(Id id) noexcept -> std::shared_ptr<MaxGauge> {
  return all_meters_.insert_max_gauge(std::move(id));
}

auto Registry::GetMaxGauge(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<MaxGauge> {
  return GetMaxGauge(Id::Of(name, std::move(tags)));
}

auto Registry::GetMonotonicCounter(Id id) noexcept
    -> std::shared_ptr<MonotonicCounter> {
  return all_meters_.insert_mono_counter(std::move(id));
}

auto Registry::GetMonotonicCounter(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<MonotonicCounter> {
  return GetMonotonicCounter(Id::Of(name, std::move(tags)));
}

auto Registry::GetMonotonicCounterUint(Id id) noexcept
    -> std::shared_ptr<MonotonicCounterUint> {
  return all_meters_.insert_mono_counter_uint(std::move(id));
}

auto Registry::GetMonotonicCounterUint(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<MonotonicCounterUint> {
  return GetMonotonicCounterUint(Id::Of(name, std::move(tags)));
}

auto Registry::GetMonotonicSampled(Id id) noexcept
    -> std::shared_ptr<MonotonicSampled> {
  return all_meters_.insert_mono_sampled(std::move(id));
}

auto Registry::GetMonotonicSampled(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<MonotonicSampled> {
  return GetMonotonicSampled(Id::Of(name, std::move(tags)));
}

auto Registry::GetTimer(Id id) noexcept -> std::shared_ptr<Timer> {
  return all_meters_.insert_timer(std::move(id));
}

auto Registry::GetTimer(std::string_view name, Tags tags) noexcept
    -> std::shared_ptr<Timer> {
  return GetTimer(Id::Of(name, std::move(tags)));
}

void Registry::Start() noexcept {
  publisher_.Start();
  if (!should_stop_.exchange(false)) {
    // already started
    return;
  }

  expirer_thread_ = std::thread(&Registry::expirer, this);
}

void Registry::Stop() noexcept {
  publisher_.Stop();
  if (!should_stop_.exchange(true)) {
    logger_->debug("Stopping expirer thread");
    cv_.notify_all();
    expirer_thread_.join();
  }
}

auto Registry::Measurements() const noexcept -> std::vector<Measurement> {
  auto res = all_meters_.measure(meter_ttl_);
  registry_size_->Record(res.size());
  for (const auto& callback : ms_callbacks_) {
    callback(res);
  }
  return res;
}

void Registry::UpdateCommonTag(const std::string& k, const std::string& v) {
  auto it = config_->common_tags.find(k);
  if (it != config_->common_tags.end()) {
    it->second = v;
  } else {
    config_->common_tags.insert({k, v});
  }
}

void Registry::EraseCommonTag(const std::string& k) {
  config_->common_tags.erase(k);
}

bool Registry::DeleteMeter(const std::string& type, const Id& id) {
  if (type == "A") {
    return all_meters_.age_gauges_.remove_one(id);
  } else if (type == "g") {
    return all_meters_.gauges_.remove_one(id);
  }
  return false;
}

void Registry::DeleteAllMeters(const std::string& type) {
  if (type == "A") {
    all_meters_.age_gauges_.remove_all();
  } else if (type == "g") {
    all_meters_.gauges_.remove_all();
  }
}

void Registry::remove_expired_meters() noexcept {
  int total = 0, expired = 0;
  std::tie(expired, total) = all_meters_.remove_expired(meter_ttl_);
  logger_->debug("Removed {} expired meters out of {} total", expired, total);
}

void Registry::expirer() noexcept {
  auto& frequency = config_->expiration_frequency;
  if (frequency == absl::ZeroDuration()) {
    logger_->info("Will not expire meters");
    return;
  }

  logger_->debug("Starting metrics expiration. Meter ttl={}s running every {}s",
                 meter_ttl_ / 1e9, absl::ToDoubleSeconds(frequency));

  // do not attempt to expire meters immediately after
  // Start - wait one freq_millis interval
  {
    std::unique_lock<std::mutex> lock{cv_mutex_};
    cv_.wait_for(lock, absl::ToChronoMilliseconds(frequency));
  }
  while (!should_stop_) {
    auto start = absl::Now();
    remove_expired_meters();
    auto elapsed = absl::Now() - start;
    if (elapsed < frequency) {
      std::unique_lock<std::mutex> lock{cv_mutex_};
      auto sleep = frequency - elapsed;
      logger_->debug("Expirer sleeping {}s", absl::ToDoubleSeconds(sleep));
      cv_.wait_for(lock, absl::ToChronoMilliseconds(sleep));
    }
  }
}
void Registry::OnMeasurements(Registry::measurements_callback fn) noexcept {
  ms_callbacks_.emplace_back(std::move(fn));
}

}  // namespace spectator
