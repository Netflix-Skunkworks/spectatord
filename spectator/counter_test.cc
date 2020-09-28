#include "../spectator/counter.h"
#include "../spectator/common_refs.h"
#include <gtest/gtest.h>

namespace {
using spectator::Measurement;
using spectator::Measurements;
using spectator::refs;

spectator::Counter getCounter(std::string_view name) {
  return spectator::Counter(spectator::Id(name, spectator::Tags{}));
}

TEST(Counter, Increment) {
  auto c = getCounter("foo");
  EXPECT_DOUBLE_EQ(c.Count(), 0.0);

  c.Increment();
  EXPECT_DOUBLE_EQ(c.Count(), 1.0);

  c.Increment();
  EXPECT_DOUBLE_EQ(c.Count(), 2.0);
}

TEST(Counter, Add) {
  auto c = getCounter("add");
  EXPECT_DOUBLE_EQ(c.Count(), 0.0);

  c.Add(42);
  EXPECT_DOUBLE_EQ(c.Count(), 42.0);

  c.Add(42);
  EXPECT_DOUBLE_EQ(c.Count(), 84.0);
}

TEST(Counter, AddFloat) {
  auto c = getCounter("add");
  EXPECT_DOUBLE_EQ(c.Count(), 0.0);

  c.Add(42.4);
  EXPECT_DOUBLE_EQ(c.Count(), 42.4);

  c.Add(0.6);
  EXPECT_DOUBLE_EQ(c.Count(), 43.0);
}

TEST(Counter, NegativeDelta) {
  auto c = getCounter("neg");
  EXPECT_DOUBLE_EQ(c.Count(), 0.0);

  c.Add(1.0);
  c.Add(-0.1);
  EXPECT_DOUBLE_EQ(c.Count(), 1.0);

  c.Add(-1.1);
  EXPECT_DOUBLE_EQ(c.Count(), 1.0);
}

TEST(Counter, Id) {
  auto c = getCounter("id");
  auto id = spectator::Id("id", spectator::Tags{});
  EXPECT_EQ(c.MeterId(), id);
}

TEST(Counter, Measure) {
  auto c = getCounter("measure");
  c.Add(42);
  EXPECT_DOUBLE_EQ(c.Count(), 42.0);

  Measurements measures;
  c.Measure(&measures);
  EXPECT_DOUBLE_EQ(c.Count(), 0.0)
      << "Counters should reset their value after being measured";

  auto counter_id = c.MeterId().WithStat(refs().count());
  std::vector<Measurement> expected({{counter_id, 42.0}});
  EXPECT_EQ(expected, measures);

  Measurements empty;
  c.Measure(&empty);
  EXPECT_TRUE(empty.empty()) << "Counters should not report 0 increments";
}

TEST(Counter, Updated) {
  auto counter = getCounter("c");
  counter.Increment();

  auto t1 = counter.Updated();
  auto t2 = counter.Updated();
  EXPECT_EQ(t1, t2);

  usleep(1);
  counter.Increment();
  EXPECT_TRUE(counter.Updated() > t1);
}
}  // namespace
