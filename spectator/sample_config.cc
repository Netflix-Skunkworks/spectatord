#include "config.h"

namespace spectator {

// used in tests and non NFLX deployments
auto GetConfiguration() -> std::unique_ptr<Config> {
  auto config = std::make_unique<Config>();
  config->age_gauge_limit = 10;
  config->process_name = "spectatord";
  config->batch_size = 10000; // Internal NFLX default
  config->frequency = absl::Seconds(5); // Internal NFLX default
  return config;
}

}  // namespace spectator

auto GetSpectatorConfig() -> std::unique_ptr<spectator::Config> {
  return spectator::GetConfiguration();
}
