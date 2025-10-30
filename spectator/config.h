#pragma once

#include "absl/time/time.h"
#include <map>
#include <memory>
#include <string>

namespace spectator {

class Config {
 public:
  virtual ~Config() = default;
  Config() = default;
  Config(const Config&) = default;
  Config(Config&&) = default;
  auto operator=(Config &&) -> Config& = default;
  auto operator=(const Config&) -> Config& = default;

  Config(std::map<std::string, std::string> c_tags, int read_to_ms,
         int connect_to_ms, int batch_sz, int freq_ms, int exp_freq_ms,
         int meter_ttl_ms, size_t age_gauge_limit, std::string publish_uri,
         std::string external_publish_uri)
      : common_tags{std::move(c_tags)},
        read_timeout{absl::Milliseconds(read_to_ms)},
        connect_timeout{absl::Milliseconds(connect_to_ms)},
        batch_size{batch_sz},
        frequency{absl::Milliseconds(freq_ms)},
        expiration_frequency{absl::Milliseconds(exp_freq_ms)},
        meter_ttl{absl::Milliseconds(meter_ttl_ms)},
        age_gauge_limit{age_gauge_limit},
        uri{std::move(publish_uri)},
        external_uri{std::move(external_publish_uri)} {}
  std::map<std::string, std::string> common_tags;
  absl::Duration read_timeout;
  absl::Duration connect_timeout;
  int batch_size{};
  absl::Duration frequency{};
  absl::Duration expiration_frequency;
  absl::Duration meter_ttl;
  size_t age_gauge_limit{};
  std::string uri;
  std::string external_uri;

  std::string metatron_dir;
  std::string process_name;
  bool external_enabled = false;
  bool status_metrics_enabled = true;
  bool verbose_http = false;

  // sub-classes can override this method implementing custom logic
  // that can disable publishing under certain conditions
  [[nodiscard]] virtual auto is_enabled() const -> bool { return true; }
};

// Get a new spectator configuration.
auto GetConfiguration() -> std::unique_ptr<Config>;

}  // namespace spectator
