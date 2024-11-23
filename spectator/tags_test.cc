#include "tags.h"
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
  t.add("A000", "V000");

  using spectator::intern_str;
  for (auto i = 15; i >= 0; --i) {
    auto k = intern_str(fmt::format("K{}", i));
    auto v = intern_str(fmt::format("V{}", i));
    EXPECT_EQ(t.at(k), v);
  }
  auto k = intern_str("A000");
  auto v = intern_str("V000");
  EXPECT_EQ(t.at(k), v);
}

// add n tags of the form K<num> = V<num>
auto prepare_tags(Tags* tags) -> void {
  for (auto i = 0; i < 16; ++i) {
    tags->add(fmt::format("K{}", i), fmt::format("V{}", i));
  }
}

auto verify_tags(const Tags& tags, int n) -> void {
  ASSERT_TRUE(tags.capacity() >= (size_t)n);
  for (auto i = n - 1; i >= 0; --i) {
    auto k = spectator::intern_str(fmt::format("K{}", i));
    auto v = spectator::intern_str(fmt::format("V{}", i));
    EXPECT_EQ(tags.at(k), v);
  }
}

TEST(Tags, Copy) {
  for (auto size = 1; size < 16; ++size) {
    Tags t;
    prepare_tags(&t);

    Tags t2{t};
    ASSERT_EQ(t2.size(), t.size());
    ASSERT_TRUE(t2.capacity() >= t2.size());
    ASSERT_TRUE(t2.capacity() >= t.size());
    verify_tags(t, size);
  }
}

TEST(Tags, Copy_Add) {
  using spectator::intern_str;
  for (auto size = 15; size < 16; ++size) {
    Tags t;
    prepare_tags(&t);
    Tags t2{t};

    // add to the end
    t2.add(fmt::format("K{}", size), fmt::format("V{}", size));

    // add to the beginning
    t2.add("A000", "V000");

    // add to the middle
    t2.add(fmt::format("K{}-", size), "V000");

    verify_tags(t2, size + 1);
    auto k = intern_str("A000");
    auto v = intern_str("V000");
    EXPECT_EQ(t2.at(k), v);

    k = intern_str(fmt::format("K{}-", size + 1));
    EXPECT_EQ(t2.at(k), t.at(v));
  }
}

}  // namespace