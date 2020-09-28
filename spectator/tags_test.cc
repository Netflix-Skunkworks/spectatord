#include "spectator/tags.h"
#include <gtest/gtest.h>
#include <fmt/format.h>

namespace {

using spectator::Tags;
TEST(Tags, Init) {
  Tags t;
  EXPECT_EQ(t.size(), 0);

  t.add("k2", "v2");
  EXPECT_EQ(t.size(), 1);
  t.add("k1", "some-value");
  EXPECT_EQ(t.size(), 2);
  t.add("k1", "v1");
  EXPECT_EQ(t.size(), 2);

  Tags t2{{"k1", "v1"}, {"k2", "v3"}};
  t2.add("k2", "v2");

  EXPECT_EQ(t, t2);
}

TEST(Tags, Hash) {
  Tags t;
  t.add("k1", "v1");
  t.add("k1", "v1.1");
  t.add("k2", "v2");

  Tags t2{{"k2", "v2"}, {"k1", "v1.1"}};
  EXPECT_EQ(t, t2);
  EXPECT_EQ(t.hash(), t2.hash());
}

TEST(Tags, At) {
  Tags t;
  for (auto i = 0; i < 16; ++i) {
    t.add(fmt::format("K{}", i), fmt::format("V{}", i));
  }

  for (auto i = 15; i >= 0; --i) {
    auto k = spectator::intern_str(fmt::format("K{}", i));
    auto v = spectator::intern_str(fmt::format("V{}", i));
    EXPECT_EQ(t.at(k), v);
  }
}
}  // namespace