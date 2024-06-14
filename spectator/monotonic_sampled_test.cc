#include "../spectator/monotonic_sampled.h"
#include <gtest/gtest.h>

namespace {

using spectator::Id;
using spectator::refs;

auto getMonotonicSampled(std::string_view name) {
  return spectator::MonotonicSampled(Id::Of(name));
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

TEST(MonotonicSampled, NegativeDelta) {
  auto c = getMonotonicSampled("neg");
  EXPECT_TRUE(std::isnan(c.SampledRate()));

  spectator::Measurements ms;
  c.Set(100.0, s_to_ns(1));
  c.Measure(&ms);
  EXPECT_TRUE(ms.empty());

  c.Set(99.0, s_to_ns(2));
  c.Measure(&ms);
  EXPECT_TRUE(ms.empty());

  c.Set(98.0, s_to_ns(3));
  c.Measure(&ms);
  EXPECT_TRUE(ms.empty());

  c.Set(100.0, s_to_ns(4));
  c.Measure(&ms);
  ASSERT_EQ(ms.size(), 1);
  auto id = c.MeterId().WithStat(refs().count());
  auto expected = spectator::Measurement{id, 2.0};
  EXPECT_EQ(expected, ms.front());
}


TEST(MonotonicSampled, Id) {
  auto c = getMonotonicSampled("id");
  auto id = Id::Of("id");
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
