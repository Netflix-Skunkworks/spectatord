#include "backward.hpp"
#include <gtest/gtest.h>

int main(int argc, char** argv) {
  using backward::SignalHandling;
  auto signals = SignalHandling::make_default_signals();
  // default signals except for SIGABRT
  signals.erase(std::remove(signals.begin(), signals.end(), SIGABRT), signals.end());
  SignalHandling sh{signals};
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
