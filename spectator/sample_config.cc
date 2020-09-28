#include "config.h"


namespace spectator {

// used in tests
std::unique_ptr<Config> GetConfiguration() {
  return std::make_unique<Config>();
}

}  // namespace spectator

std::unique_ptr<spectator::Config> GetSpectatorConfig() {
  return spectator::GetConfiguration(); 
}

