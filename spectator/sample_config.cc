#include "config.h"

namespace spectator {

// used in tests
auto GetConfiguration() -> std::shared_ptr<Config> {
  auto config = std::make_shared<Config>();
  config->age_gauge_limit = 10;
  return config;
}

}  // namespace spectator

auto GetSpectatorConfig() -> std::shared_ptr<spectator::Config> {
  return spectator::GetConfiguration();
}
