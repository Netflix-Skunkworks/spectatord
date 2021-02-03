#include "../spectator/max_gauge.h"
#include "../spectator/common_refs.h"
#include <gtest/gtest.h>

namespace {

using spectator::Measurement;
using spectator::refs;

spectator::MaxGauge getMaxGauge(std::string_view name) {
  return spectator::MaxGauge(spectator::Id(name, spectator::Tags{}));
}

TEST(MaxGauge, Init) {
  auto g = getMaxGauge("foo");
  EXPECT_TRUE(std::isnan(g.Get()));

  g.Set(42);
  EXPECT_DOUBLE_EQ(42.0, g.Get());

  spectator::Measurements ms;
  g.Measure(&ms);
  ASSERT_EQ(ms.size(), 1);
  auto id = g.MeterId().WithStat(refs().max());
  auto expected = Measurement{id, 42.0};
  EXPECT_EQ(expected, ms.front());
}

TEST(MaxGauge, MaxUpdates) {
  auto g = getMaxGauge("max");
  g.Update(100.0);
  EXPECT_DOUBLE_EQ(g.Get(), 100.0);
  g.Update(101.0);
  EXPECT_DOUBLE_EQ(g.Get(), 101.0);
  g.Update(100.0);
  EXPECT_DOUBLE_EQ(g.Get(), 101.0);
}

TEST(MaxGauge, NegativeValues) {
  auto g = getMaxGauge("max");
  g.Update(-2);
  EXPECT_DOUBLE_EQ(g.Get(), -2);
}

TEST(MaxGauge, Id) {
  auto g = getMaxGauge("id");
  auto id = spectator::Id("id", spectator::Tags{});
  EXPECT_EQ(g.MeterId(), id);
}

TEST(MaxGauge, Measure) {
  auto g = getMaxGauge("measure");

  spectator::Measurements ms;
  g.Measure(&ms);
  ASSERT_TRUE(ms.empty());  // initialize

  g.Update(42);
  g.Measure(&ms);
  ASSERT_EQ(ms.size(), 1);
  auto id = g.MeterId().WithStat(refs().max());
  std::vector<Measurement> expected({{id, 42.0}});
  EXPECT_EQ(expected, ms);

  ms.clear();
  g.Measure(&ms);
  ASSERT_TRUE(ms.empty());  // reset after measurement

  g.Update(4.0);
  g.Measure(&ms);
  expected.at(0).value = 4.0;
  EXPECT_EQ(expected, ms);
}

TEST(MaxGauge, Updated) {
  auto m = getMaxGauge("m");
  m.Update(1);

  auto t1 = m.Updated();
  auto t2 = m.Updated();
  EXPECT_EQ(t1, t2);

  usleep(1);
  m.Set(2);
  EXPECT_TRUE(m.Updated() > t1);
}

}  // namespace
