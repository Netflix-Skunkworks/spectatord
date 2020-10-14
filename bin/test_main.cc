#include "backward.hpp"
#include <gtest/gtest.h>

int main(int argc, char** argv) {
  backward::SignalHandling sh;
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}