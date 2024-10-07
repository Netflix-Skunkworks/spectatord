#pragma once

#include "absl/time/time.h"
#include "absl/synchronization/blocking_counter.h"
#include "common_refs.h"
#include "config.h"
#include "counter.h"
#include "http_client.h"
#include "util/logger.h"
#include "measurement.h"
#include "metatron/metatron_config.h"
#include "smile.h"

#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <rapidjson/document.h>
#include <ska/flat_hash_map.hpp>
#include <thread>
#include <tsl/hopscotch_set.h>

namespace spectator {

namespace detail {
template <typename R>
auto get_counter(R* registry, Tags tags) -> std::shared_ptr<Counter> {
  static constexpr auto kSpectatorMeasurements = "spectator.measurements";
  tags.add("owner", "spectatord");
  return registry->GetCounter(kSpectatorMeasurements, std::move(tags));
}
}  // namespace detail

template <typename R>
class Publisher {
 public:
  explicit Publisher(R* registry)
      : registry_{registry},
        started_{false},
        should_stop_{false},
        last_successful_send_{absl::GetCurrentTimeNanos()},
        sentMetrics_{detail::get_counter(registry, Tags{{"id", "sent"}})},
        invalidMetrics_{detail::get_counter(
            registry, Tags{{"id", "dropped"}, {"error", "validation"}})},
        droppedHttp_{detail::get_counter(
            registry, Tags{{"id", "dropped"}, {"error", "http-error"}})},
        droppedOther_{detail::get_counter(
            registry, Tags{{"id", "dropped"}, {"error", "other"}})},
        num_sender_threads_{std::min(8U, std::thread::hardware_concurrency())},
        pool_{num_sender_threads_} {
    buffers_.resize(num_sender_threads_);
    for (const auto& kv : registry_->GetConfig().common_tags) {
      common_tags_.add(kv.first, kv.second);
    }
  }

  Publisher(const Publisher&) = delete;
  Publisher(Publisher&&) = delete;
  auto operator=(const Publisher&) -> Publisher& = delete;
  auto operator=(Publisher &&) -> Publisher& = delete;

  ~Publisher() {
    if (started_) {
      Stop();
    }
  }
  void Start() {
    if (!http_initialized_) {
      http_initialized_ = true;
      HttpClient::GlobalInit();
    }
    const auto& cfg = registry_->GetConfig();
    auto logger = registry_->GetLogger();
    if (cfg.uri.empty()) {
      logger->warn("Registry config has no uri. Ignoring start request");
      return;
    }
    if (started_.exchange(true)) {
      logger->warn("Registry already started. Ignoring start request");

      return;
    }

    sender_thread_ = std::thread(&Publisher::sender, this);
  }

  auto GetLastSuccessTime() const -> int64_t {
    return last_successful_send_.load(std::memory_order_relaxed);
  }

  void Stop() {
    if (started_.exchange(false)) {
      should_stop_ = true;
      cv_.notify_all();
      sender_thread_.join();
    }

    if (http_initialized_.exchange(false)) {
      HttpClient::GlobalShutdown();
    }
  }

 private:
  R* registry_;
  std::atomic<bool> started_;
  std::atomic<bool> http_initialized_{false};
  std::atomic<bool> should_stop_;
  std::atomic<int64_t> last_successful_send_;
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  std::thread sender_thread_;
  std::shared_ptr<Counter> sentMetrics_;
  std::shared_ptr<Counter> invalidMetrics_;
  std::shared_ptr<Counter> droppedHttp_;
  std::shared_ptr<Counter> droppedOther_;
  Tags common_tags_;
  size_t num_sender_threads_;
  asio::thread_pool pool_;
  std::vector<SmilePayload> buffers_;

  void sender() noexcept {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    const auto& cfg = registry_->GetConfig();
    auto logger = registry_->GetLogger();

    logger->info("Starting to send metrics to {} every {}s.", cfg.uri,
                 absl::ToDoubleSeconds(cfg.frequency));
    logger->info("Publishing metrics with the following common tags: {}",
                 common_tags_);

    while (!should_stop_) {
      auto start = absl::Now();
      try {
        send_metrics();
      } catch (std::exception& e) {
        logger->error("Ignoring exception while sending metrics: {}", e.what());
      }
      auto elapsed = absl::Now() - start;

      if (elapsed < cfg.frequency) {
        std::unique_lock<std::mutex> lock{cv_mutex_};
        auto sleep = cfg.frequency - elapsed;
        logger->debug("Sleeping {}s until the next interval",
                      absl::ToDoubleSeconds(sleep));
        cv_.wait_for(lock, absl::ToChronoMilliseconds(sleep));
      }
    }
    logger->info("Stopping Publisher");
  }

  // for testing
 protected:
  using StrTable = ska::flat_hash_map<StrRef, int>;
  auto build_str_table(SmilePayload* payload,
                       std::vector<Measurement>::const_iterator first,
                       std::vector<Measurement>::const_iterator last)
      -> StrTable {
    StrTable strings{static_cast<StrTable::size_type>((last - first) * 7)};
    for (const auto& tag : common_tags_) {
      strings[tag.key] = 0;
      strings[tag.value] = 0;
    }
    strings[refs().name()] = 0;
    for (auto it = first; it != last; ++it) {
      const auto& m = *it;
      strings[m.id.Name()] = 0;
      for (const auto& tag : m.id.GetTags()) {
        strings[tag.key] = 0;
        strings[tag.value] = 0;
      }
    }
    auto idx = 0;
    for (auto& kv : strings) {
      kv.second = idx++;
    }

    payload->Append(strings.size());
    for (const auto& s : strings) {
      payload->Append(s.first.Get());
    }
    return strings;
  }

  enum class Op { Add = 0, Max = 10 };

  auto op_from_tags(const Tags& tags) -> Op {
    auto stat = tags.at(refs().statistic());
    if (stat == refs().count() || stat == refs().totalAmount() ||
        stat == refs().totalTime() || stat == refs().totalOfSquares() ||
        stat == refs().percentile()) {
      return Op::Add;
    }
    return Op::Max;
  }

  template <typename T>
  void add_tags(SmilePayload* payload, const StrTable& strings, const T& tags) {
    for (const auto& tag : tags) {
      auto k_pair = strings.find(tag.key);
      auto v_pair = strings.find(tag.value);
      assert(k_pair != strings.end());
      assert(v_pair != strings.end());
      payload->Append(k_pair->second);
      payload->Append(v_pair->second);
    }
  }

  void append_measurement(SmilePayload* payload, const StrTable& strings,
                          const std::vector<int>& common_ids,
                          const Measurement& m) {
    auto op = op_from_tags(m.id.GetTags());
    auto common_tags_size = common_ids.size() / 2;
    auto total_tags = m.id.GetTags().size() + 1 + common_tags_size;
    payload->Append(total_tags);
    for (auto i : common_ids) {
      payload->Append(i);
    }
    add_tags(payload, strings, m.id.GetTags());
    auto name_idx = strings.find(refs().name())->second;
    auto name_value_idx = strings.find(m.id.Name())->second;
    payload->Append(name_idx);
    payload->Append(name_value_idx);
    auto op_num = static_cast<int>(op);
    payload->Append(op_num);
    payload->Append(m.value);
  }

  auto get_common_ids(const StrTable& strings) -> std::vector<int> {
    std::vector<int> ids;
    ids.reserve(common_tags_.size() * 2);
    for (const auto& tag : common_tags_) {
      auto key_pair = strings.find(tag.key);
      auto val_pair = strings.find(tag.value);
      assert(key_pair != strings.end());
      assert(val_pair != strings.end());
      ids.emplace_back(key_pair->second);
      ids.emplace_back(val_pair->second);
    }
    return ids;
  }

  void measurements_to_json(SmilePayload* payload,
                            std::vector<Measurement>::const_iterator first,
                            std::vector<Measurement>::const_iterator last) {
    payload->Init();
    auto strings = build_str_table(payload, first, last);
    auto common_ids = get_common_ids(strings);
    for (auto it = first; it != last; ++it) {
      append_measurement(payload, strings, common_ids, *it);
    }
  }

  static auto get_http_config(const Config& cfg) -> HttpClientConfig {
    auto read_timeout = cfg.read_timeout;
    auto connect_timeout = cfg.connect_timeout;
    if (read_timeout == absl::ZeroDuration()) {
      read_timeout = absl::Seconds(3);
    }
    if (connect_timeout == absl::ZeroDuration()) {
      connect_timeout = absl::Seconds(2);
    }
    auto static cert_info = metatron::find_certificate(cfg.external_enabled, cfg.metatron_dir);
    return HttpClientConfig{connect_timeout, read_timeout, true, false, true,
                            cfg.verbose_http, cfg.status_metrics_enabled,
                            cfg.external_enabled, cert_info};
  }

  auto handle_aggr_response(const HttpResponse& http_response,
                            size_t num_measurements,
                            tsl::hopscotch_set<std::string>* err_messages)
      -> std::pair<size_t, size_t> {
    size_t num_sent = 0U;
    size_t num_err = 0U;
    auto logger = registry_->GetLogger();
    const auto& uri = registry_->GetConfig().uri;
    const auto& status_metrics_enabled = registry_->GetConfig().status_metrics_enabled;
    auto http_code = http_response.status;
    if (http_code == 200) {
      num_sent = num_measurements;
    } else if (http_code > 200 && http_code < 500) {
      rapidjson::Document body;
      body.Parse(http_response.raw_body.c_str(), http_response.raw_body.size());
      if (body.HasParseError()) {
        logger->error("Unable to parse JSON response from {} - status {}: {}",
                      uri, http_code, http_response.raw_body);
        num_err = num_measurements;
        if (status_metrics_enabled) {
          droppedOther_->Add(num_measurements);
        }
      } else {
        if (body.HasMember("errorCount")) {
          auto err_count = body["errorCount"].GetInt();
          num_err = err_count;
          num_sent = num_measurements - err_count;
          if (status_metrics_enabled) {
            invalidMetrics_->Add(err_count);
            sentMetrics_->Add(static_cast<double>(num_measurements - err_count));
          }
          auto messages = body["message"].GetArray();
          for (auto& msg : messages) {
            err_messages->emplace(
                std::string(msg.GetString(), msg.GetStringLength()));
          }
        } else {
          logger->error("Missing errorCount field in JSON response from {}: {}",
                        uri, http_code, http_response.raw_body);
          if (status_metrics_enabled) {
            droppedOther_->Add(num_measurements);
          }
          num_err = num_measurements;
        }
      }
    } else if (http_code == -1) {  // connection error or timeout
      num_err = num_measurements;
      if (status_metrics_enabled) {
        droppedOther_->Add(num_measurements);
      }
    } else {  // 5xx error
      if (status_metrics_enabled) {
        droppedHttp_->Add(num_measurements);
      }
      num_err = num_measurements;
    }
    return std::make_pair(num_sent, num_err);
  }

  void send_metrics() {
    auto logger = registry_->GetLogger();
    const auto& cfg = registry_->GetConfig();
    auto http_cfg = get_http_config(cfg);
    auto start = absl::Now();
    HttpClient client{registry_, std::move(http_cfg)};
    auto batch_size = static_cast<std::vector<Measurement>::difference_type>(cfg.batch_size);
    auto measurements = registry_->Measurements();

    if (!cfg.is_enabled() || measurements.empty()) {
      if (logger->should_log(spdlog::level::trace)) {
        logger->trace("Skip sending metrics: ATLAS_DISABLED_FILE exists or measurements is empty");
      }
      return;
    }

    if (logger->should_log(spdlog::level::trace)) {
      logger->trace("Sending {} measurements to {}",
                    measurements.size(), registry_->GetConfig().uri);
      for (const auto& m : measurements) {
        logger->trace("{}", m);
      }
    }

    auto from = measurements.begin();
    auto end = measurements.end();
    std::vector<std::pair<int, HttpResponse>> responses;

    absl::Mutex responses_mutex;
    absl::Mutex buffers_mutex;
    std::vector<SmilePayload*> avail_buffers;
    std::transform(buffers_.begin(), buffers_.end(),
                   std::back_inserter(avail_buffers),
                   [](auto& b) { return &b; });
    std::vector<std::pair<Measurements::const_iterator, Measurements::const_iterator>> batches;

    while (from != end) {
      auto to_end = std::distance(from, end);
      auto to_advance = std::min(batch_size, to_end);
      auto to = from;
      std::advance(to, to_advance);
      batches.emplace_back(from, to);
      from = to;
    }
    absl::BlockingCounter batches_to_do{static_cast<int>(batches.size())};

    for (const auto& batch : batches) {
      asio::post(pool_, [this, batch, &batches_to_do, &client, &responses,
                         &buffers_mutex, &avail_buffers, &responses_mutex]() {
        const auto& uri = this->registry_->GetConfig().uri;
        SmilePayload* payload = nullptr;
        {
          absl::MutexLock lock(&buffers_mutex);
          payload = avail_buffers.back();
          avail_buffers.pop_back();
        }

        measurements_to_json(payload, batch.first, batch.second);
        auto response = client.Post(uri, HttpClient::kSmileJson, payload->Result());
        {
          absl::MutexLock lock(&responses_mutex);
          auto batch_size = batch.second - batch.first;
          if (response.status / 100 == 2) {
            last_successful_send_ = absl::GetCurrentTimeNanos();
          }
          responses.emplace_back(batch_size, std::move(response));
        }
        {
          absl::MutexLock lock(&buffers_mutex);
          avail_buffers.emplace_back(payload);
        }
        batches_to_do.DecrementCount();
      });
    }
    batches_to_do.Wait();

    auto num_err = 0U;
    auto num_sent = 0U;
    tsl::hopscotch_set<std::string> err_messages;
    for (const auto& resp_pair : responses) {
      size_t batch_sent = 0;
      size_t batch_err = 0;
      std::tie(batch_sent, batch_err) = handle_aggr_response(
          resp_pair.second, resp_pair.first, &err_messages);
      num_sent += batch_sent;
      num_err += batch_err;
    }

    auto elapsed = absl::Now() - start;
    if (num_err > 0) {
      logger->info("Sent: {} Dropped: {} Total: {}. Elapsed {:.3f}s", num_sent,
                    num_err, measurements.size(), absl::ToDoubleSeconds(elapsed));
    } else {
      logger->debug("Sent: {} Dropped: {} Total: {}. Elapsed {:.3f}s", num_sent,
                    num_err, measurements.size(), absl::ToDoubleSeconds(elapsed));
    }
    for (const auto& m : err_messages) {
      logger->info("Validation error: {}", m);
    }
  }
};

}  // namespace spectator
