#include "config.h"

namespace spectator {

// used in tests
auto GetConfiguration() -> std::unique_ptr<Config> {
  return std::make_unique<Config>();
}

}  // namespace spectator

auto GetSpectatorConfig() -> std::unique_ptr<spectator::Config> {
  return spectator::GetConfiguration();
}
