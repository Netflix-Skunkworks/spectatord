#include "config.h"

namespace spectator {

std::unique_ptr<Config> GetConfiguration() {
  return std::make_unique<Config>();
}

}  // namespace spectator
