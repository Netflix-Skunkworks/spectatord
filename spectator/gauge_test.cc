#include "../spectator/gauge.h"
#include "spectator/common_refs.h"
#include <gtest/gtest.h>

namespace {
using spectator::Gauge;
auto getGauge(std::string_view name) -> Gauge {
  return Gauge(spectator::Id(name, spectator::Tags{}), absl::Minutes(1));
}

TEST(Gauge, Init) {
  auto g = getGauge("g");
  auto v = g.Get();
  EXPECT_TRUE(std::isnan(v));
}

TEST(Gauge, Set) {
  auto g = getGauge("g");
  g.Set(42.0);
  EXPECT_DOUBLE_EQ(42.0, g.Get());
}

TEST(Gauge, Measure) {
  auto g = getGauge("m");
  g.Set(1.0);
  spectator::Measurements ms;
  g.Measure(&ms);
  auto id = g.MeterId().WithStat(spectator::refs().gauge());

  using spectator::Measurement;
  auto expected = std::vector<Measurement>({{id, 1.0}});
  EXPECT_EQ(expected, ms);

  auto v = g.Get();
  // reset behavior
  EXPECT_FALSE(std::isnan(v)) << "Gauges should only reset after expiration";
}

TEST(Gauge, Updated) {
  auto gauge = getGauge("g");
  gauge.Set(1);

  auto t1 = gauge.Updated();
  auto t2 = gauge.Updated();
  EXPECT_EQ(t1, t2);

  usleep(1);
  gauge.Set(1);
  EXPECT_TRUE(gauge.Updated() > t1);
}

}  // namespace
