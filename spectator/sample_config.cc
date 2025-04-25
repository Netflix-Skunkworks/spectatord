#include "config.h"

namespace spectator {

// used in tests
auto GetConfiguration() -> std::unique_ptr<Config> {
  auto config = std::make_unique<Config>();
  config->age_gauge_limit = 10;
  config->process_name = "spectatord";
  return config;
}

}  // namespace spectator

auto GetSpectatorConfig() -> std::unique_ptr<spectator::Config> {
  return spectator::GetConfiguration();
}
