#include "dist_summary.h"
#include "common_refs.h"
#include <gtest/gtest.h>
#include <unordered_map>

namespace {
using spectator::DistributionSummary;

DistributionSummary getDS() {
  return DistributionSummary(spectator::Id::Of("ds"));
}

TEST(DistributionSummary, Record) {
  auto ds = getDS();
  // quick sanity check
  EXPECT_EQ(ds.Count(), 0);
  EXPECT_EQ(ds.TotalAmount(), 0);

  ds.Record(100);
  EXPECT_EQ(ds.Count(), 1);
  EXPECT_EQ(ds.TotalAmount(), 100);

  ds.Record(0);
  EXPECT_EQ(ds.Count(), 2);
  EXPECT_EQ(ds.TotalAmount(), 100);

  ds.Record(101);
  EXPECT_EQ(ds.Count(), 3);
  EXPECT_EQ(ds.TotalAmount(), 201);

  ds.Record(-1);
  EXPECT_EQ(ds.Count(), 3);
  EXPECT_EQ(ds.TotalAmount(), 201);
}

auto expect_dist_summary(const DistributionSummary& ds, int64_t count,
                         int64_t total, double total_sq, int64_t max) -> void {
  using spectator::refs;
  spectator::Measurements ms;
  ds.Measure(&ms);
  ASSERT_EQ(ms.size(), 4)
      << "Expected 4 measurements from a DistributionSummary, got "
      << ms.size();

  std::unordered_map<spectator::Id, double> expected;
  const auto& id = ds.MeterId();
  expected[id.WithStat(refs().count())] = count;
  expected[id.WithStat(refs().totalAmount())] = total;
  expected[id.WithStat(refs().totalOfSquares())] = total_sq;
  expected[id.WithStat(refs().max())] = max;

  for (const auto& m : ms) {
    auto it = expected.find(m.id);
    ASSERT_TRUE(it != expected.end()) << "Unexpected result: " << m;
    EXPECT_DOUBLE_EQ(it->second, m.value)
        << "Wrong value for measurement " << m << " expected " << it->second;
    expected.erase(it);  // done processing this entry
  }
  ASSERT_EQ(expected.size(), 0);

  ms.clear();
  ds.Measure(&ms);
  EXPECT_TRUE(ms.empty()) << "Resets after measuring";
}

TEST(DistributionSummary, Measure) {
  auto ds = getDS();
  spectator::Measurements ms;
  ds.Measure(&ms);
  EXPECT_TRUE(ms.empty());

  ds.Record(100);
  ds.Record(200);
  expect_dist_summary(ds, 2, 300, 100 * 100 + 200 * 200, 200);
}

TEST(DistributionSummary, Updated) {
  auto ds = getDS();
  ds.Record(1);

  auto t1 = ds.Updated();
  auto t2 = ds.Updated();
  EXPECT_EQ(t1, t2);

  usleep(1);
  ds.Record(1);
  EXPECT_TRUE(ds.Updated() > t1);
}

}  // namespace
