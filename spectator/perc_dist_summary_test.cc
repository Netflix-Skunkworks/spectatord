#include "percentile_distribution_summary.h"
#include "percentile_bucket_tags.inc"
#include "test_utils.h"
#include <fmt/ostream.h>
#include <gtest/gtest.h>

namespace {
using namespace spectator;

template <class T>
std::unique_ptr<T> getDS(Registry* r) {
  return std::make_unique<T>(r, Id::Of("ds"), 0, 1000 * 1000);
}

using Implementations = testing::Types<PercentileDistributionSummary>;

// satisfy -Wgnu-zero-variadic-macro-arguments in TYPED_TEST_SUITE
class NameGenerator {
 public:
  template <typename T>
  static std::string GetName(int) {
    if constexpr (std::is_same_v<T, PercentileDistributionSummary>) return "PercentileDistributionSummary";
    return "unknownType";
  }
};

template <class T>
class PercentileDistributionSummaryTest : public ::testing::Test {
 protected:
  PercentileDistributionSummaryTest()
      : r{GetConfiguration(), spectatord::Logger()},
        ds{getDS<T>(&r)},
        restricted_ds{&r, Id::Of("ds2"), 5, 2000} {}

  Registry r;
  std::unique_ptr<T> ds;
  T restricted_ds;
};

TYPED_TEST_SUITE(PercentileDistributionSummaryTest, Implementations, NameGenerator);

TYPED_TEST(PercentileDistributionSummaryTest, Measure) {
  this->ds->Record(42);

  auto ms = this->r.Measurements();
  auto actual = measurements_to_map(ms);
  auto expected = std::map<std::string, double>{};

  auto percentileTag = kDistTags.at(PercentileBucketIndexOf(42));
  expected[fmt::format("ds|percentile={}|statistic=percentile", percentileTag)] = 1;
  expected["ds|statistic=count"] = 1;
  expected["ds|statistic=max"] = 42;
  expected["ds|statistic=totalAmount"] = 42;
  expected["ds|statistic=totalOfSquares"] = 42 * 42;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first]);
  }
}

TYPED_TEST(PercentileDistributionSummaryTest, CountTotal) {
  for (auto i = 0; i < 100; ++i) {
    this->ds->Record(i);
  }

  EXPECT_EQ(this->ds->Count(), 100);
  EXPECT_EQ(this->ds->TotalAmount(), 100 * 99 / 2);  // sum(1,n) = n * (n - 1) / 2
}

TYPED_TEST(PercentileDistributionSummaryTest, Restrict) {
  auto& ds = this->restricted_ds;

  ds.Record(-1);
  ds.Record(0);
  ds.Record(10);
  ds.Record(10000);

  auto total = 0 + 10 + 10000;  // we ignore the negative value
  EXPECT_EQ(ds.TotalAmount(), total);
  EXPECT_EQ(ds.Count(), 3);

  auto measurements = this->r.Measurements();
  auto actual = measurements_to_map(measurements);
  auto expected = std::map<std::string, double>{};

  auto minPercTag = kDistTags.at(PercentileBucketIndexOf(5));
  auto maxPercTag = kDistTags.at(PercentileBucketIndexOf(2000));
  auto percTag = kDistTags.at(PercentileBucketIndexOf(10));

  auto name = ds.MeterId().Name().Get();
  expected[fmt::format("{}|percentile={}|statistic=percentile", name, minPercTag)] = 1;
  expected[fmt::format("{}|percentile={}|statistic=percentile", name, maxPercTag)] = 1;
  expected[fmt::format("{}|percentile={}|statistic=percentile", name, percTag)] = 1;

  expected[fmt::format("{}|statistic=count", name)] = 3;
  auto totalSq = 10 * 10 + 10000 * 10000;
  expected[fmt::format("{}|statistic=max", name)] = 10000;
  expected[fmt::format("{}|statistic=totalAmount", name)] = total;
  expected[fmt::format("{}|statistic=totalOfSquares", name)] = totalSq;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first]) << expected_m.first;
  }
}
}  // namespace
