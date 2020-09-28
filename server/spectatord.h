#pragma once

#include "expiring_cache.h"
#include "handler.h"
#include "spectator/percentile_distribution_summary.h"
#include "spectator/percentile_timer.h"
#include "spectator/registry.h"

namespace spectatord {

class Server {
 public:
  Server(int port_number, int statsd_port_number, std::string socket_path,
         spectator::Registry* registry);
  Server(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(const Server&) = delete;
  Server& operator=(Server&&) = delete;

  ~Server() = default;

  void Start();
  void Stop();

 private:
  int port_number_;
  int statsd_port_number_;
  std::string socket_path_;
  spectator::Registry* registry_;
  std::shared_ptr<spectator::Counter> parsed_count_;
  std::shared_ptr<spectator::Counter> parse_errors_;
  std::shared_ptr<spdlog::logger> logger_;
  expiring_cache<spectator::PercentileTimer> perc_timers_;
  expiring_cache<spectator::PercentileDistributionSummary> perc_ds_;

  std::atomic_bool should_stop_{false};
  std::thread upkeep_thread_;  // some janitorial tasks, like expiration of
                               // cache entries and generation of metrics
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  void upkeep();
  void update_network_metrics();

  std::optional<std::string> parse_lines(char* buffer, const handler_t& parser);
  std::optional<std::string> parse_line(const char* buffer);
  std::optional<std::string> parse_statsd_line(const char* buffer);

 protected:
  std::optional<std::string> parse(char* buffer);
  std::optional<std::string> parse_statsd(char* buffer);
};

struct measurement {
  spectator::Id id;
  double value;
};

std::optional<measurement> get_measurement(const char* measurement_str,
                                           std::string* err_msg);

}  // namespace spectatord
