#include "logger.h"
#include "registry.h"
#include <fmt/ostream.h>

#include <utility>

namespace spectator {

Registry::Registry(std::unique_ptr<Config> config,
                   Registry::logger_ptr logger) noexcept
    : should_stop_{true},
      meter_ttl_{absl::ToInt64Nanoseconds(config->meter_ttl)},
      config_{std::move(config)},
      logger_{std::move(logger)},
      registry_size_{GetDistributionSummary("spectator.registrySize")},
      publisher_(this) {
  if (meter_ttl_ == 0) {
    meter_ttl_ = int64_t(15) * 60 * 1000 * 1000 * 1000;
  }
}

const Config& Registry::GetConfig() const noexcept { return *config_; }

Registry::logger_ptr Registry::GetLogger() const noexcept { return logger_; }

std::shared_ptr<Counter> Registry::GetCounter(Id id) noexcept {
  return all_meters_.insert_counter(std::move(id));
}

std::shared_ptr<Counter> Registry::GetCounter(std::string_view name,
                                              Tags tags) noexcept {
  return GetCounter(Id::Of(name, std::move(tags)));
}

std::shared_ptr<DistributionSummary> Registry::GetDistributionSummary(
    Id id) noexcept {
  return all_meters_.insert_dist_sum(std::move(id));
}

std::shared_ptr<DistributionSummary> Registry::GetDistributionSummary(
    std::string_view name, Tags tags) noexcept {
  return GetDistributionSummary(Id::Of(name, std::move(tags)));
}

std::shared_ptr<Gauge> Registry::GetGauge(Id id) noexcept {
  return all_meters_.insert_gauge(std::move(id), GetConfig().meter_ttl);
}

std::shared_ptr<Gauge> Registry::GetGauge(Id id, absl::Duration ttl) noexcept {
  auto g = all_meters_.insert_gauge(std::move(id), ttl);
  g->SetTtl(ttl);  // in case the previous ttl was different
  return g;
}

std::shared_ptr<Gauge> Registry::GetGauge(std::string_view name,
                                          Tags tags) noexcept {
  return GetGauge(Id::Of(name, std::move(tags)));
}

std::shared_ptr<MaxGauge> Registry::GetMaxGauge(Id id) noexcept {
  return all_meters_.insert_max_gauge(std::move(id));
}

std::shared_ptr<MaxGauge> Registry::GetMaxGauge(std::string_view name,
                                                Tags tags) noexcept {
  return GetMaxGauge(Id::Of(name, std::move(tags)));
}

std::shared_ptr<MonotonicCounter> Registry::GetMonotonicCounter(
    Id id) noexcept {
  return all_meters_.insert_mono_counter(std::move(id));
}

std::shared_ptr<MonotonicCounter> Registry::GetMonotonicCounter(
    std::string_view name, Tags tags) noexcept {
  return GetMonotonicCounter(Id::Of(name, std::move(tags)));
}

std::shared_ptr<MonotonicSampled> Registry::GetMonotonicSampled(
    Id id) noexcept {
  return all_meters_.insert_mono_sampled(std::move(id));
}

std::shared_ptr<MonotonicSampled> Registry::GetMonotonicSampled(
    std::string_view name, Tags tags) noexcept {
  return GetMonotonicSampled(Id::Of(name, std::move(tags)));
}

std::shared_ptr<Timer> Registry::GetTimer(Id id) noexcept {
  return all_meters_.insert_timer(std::move(id));
}

std::shared_ptr<Timer> Registry::GetTimer(std::string_view name,
                                          Tags tags) noexcept {
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

std::vector<Measurement> Registry::Measurements() const noexcept {
  auto res = all_meters_.measure(meter_ttl_);
  registry_size_->Record(res.size());
  for (const auto& callback : ms_callbacks_) {
    callback(res);
  }
  return res;
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
