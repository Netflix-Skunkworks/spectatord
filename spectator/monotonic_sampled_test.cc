#include "../spectator/monotonic_sampled.h"
#include <gtest/gtest.h>

namespace {

using spectator::refs;

auto getMonotonicSampled(std::string_view name) {
  return spectator::MonotonicSampled(spectator::Id::Of(name));
}

auto s_to_ns(int seconds) { return int64_t(1000) * 1000 * 1000 * seconds; }

TEST(MonotonicSampled, Init) {
  auto c = getMonotonicSampled("foo");
  EXPECT_TRUE(std::isnan(c.SampledRate()));

  c.Set(42, s_to_ns(1));
  EXPECT_TRUE(std::isnan(c.SampledRate()));

  spectator::Measurements ms;
  c.Measure(&ms);
  ASSERT_TRUE(ms.empty());

  c.Set(84, s_to_ns(2));
  EXPECT_DOUBLE_EQ(c.SampledRate(), 42.0);
}

TEST(MonotonicSampled, Overflow) {
  auto max = std::numeric_limits<uint64_t>::max();
  auto c = getMonotonicSampled("neg");
  EXPECT_TRUE(std::isnan(c.SampledRate()));

  // initialize overflow condition
  spectator::Measurements ms;
  c.Set(max - 5, s_to_ns(1));
  c.Measure(&ms);
  EXPECT_TRUE(ms.empty());

  // trigger overflow condition
  c.Set(max + 1, s_to_ns(2));
  c.Measure(&ms);
  EXPECT_EQ(ms.size(), 1);
  auto id = c.MeterId().WithStat(refs().count());
  auto expected1 = spectator::Measurement{id, 6.0};
  EXPECT_EQ(expected1, ms.back());

  // normal increment conditions
  c.Set(max + 2, s_to_ns(3));
  c.Measure(&ms);
  EXPECT_EQ(ms.size(), 2);
  auto expected2 = spectator::Measurement{id, 1.0};
  EXPECT_EQ(expected2, ms.back());

  c.Set(5, s_to_ns(4));
  c.Measure(&ms);
  EXPECT_EQ(ms.size(), 3);
  auto expected3 = spectator::Measurement{id, 4.0};
  EXPECT_EQ(expected3, ms.back());
}

TEST(MonotonicSampled, Id) {
  auto c = getMonotonicSampled("id");
  auto id = spectator::Id::Of("id");
  EXPECT_EQ(c.MeterId(), id);
}

TEST(MonotonicSampled, Measure) {
  auto c = getMonotonicSampled("measure");
  c.Set(42, s_to_ns(4));
  spectator::Measurements measures;
  c.Measure(&measures);
  ASSERT_TRUE(measures.empty());  // initialize

  EXPECT_TRUE(std::isnan(c.SampledRate()))
      << "MonotonicSamples should reset their value after being measured";

  c.Set(84.0, s_to_ns(5));
  c.Measure(&measures);
  using spectator::Measurement;
  auto id = c.MeterId().WithStat(refs().count());
  std::vector<Measurement> expected({{id, 42.0}});
  EXPECT_EQ(expected, measures);

  measures.clear();
  c.Measure(&measures);
  EXPECT_TRUE(measures.empty())
      << "MonotonicSampleds should not report delta=0";
}

TEST(MonotonicSampled, Update) {
  auto counter = getMonotonicSampled("m");
  counter.Set(1, s_to_ns(1));

  auto t1 = counter.Updated();
  auto t2 = counter.Updated();
  EXPECT_EQ(t1, t2);
  usleep(1);
  counter.Set(2, s_to_ns(2));
  EXPECT_TRUE(counter.Updated() > t1);
}
}  // namespace
