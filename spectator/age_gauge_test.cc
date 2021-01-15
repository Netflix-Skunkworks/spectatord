#include "age_gauge.h"

#include <gtest/gtest.h>

namespace {

using spectator::AgeGauge;
using spectator::Id;

TEST(AgeGauge, Init) {
  AgeGauge g{Id::Of("age")};
  auto now = absl::GetCurrentTimeNanos();
  // was not successful yet
  EXPECT_EQ(g.GetLastSuccess(), 0);
  auto seconds = static_cast<double>(now) / 1e9;
  EXPECT_DOUBLE_EQ(g.Value(now), seconds);
}

TEST(AgeGauge, Update) {
  AgeGauge g{Id::Of("age")};
  auto now = absl::GetCurrentTimeNanos();
  g.UpdateLastSuccess(now);
  EXPECT_DOUBLE_EQ(g.Value(now), 0.0);

  now += int64_t{1000} * 1000 * 1000;
  EXPECT_DOUBLE_EQ(g.Value(now), 1.0);
}

TEST(AgeGauge, Measure) {
  AgeGauge g{Id::Of("age")};
  auto now = absl::GetCurrentTimeNanos();
  g.UpdateLastSuccess(now);

  spectator::Measurements results;
  g.Measure(&results);
  ASSERT_EQ(results.size(), 1);

  auto& m = results.front();
  EXPECT_EQ(m.id, Id::Of("age", {{"statistic", "gauge"}}));

  // should be close to 0
  EXPECT_TRUE(m.value > 0.0 && m.value < 0.1);
}

}  // namespace
