#include <gtest/gtest.h>
#include "../spectator/percentile_timer.h"
#include "percentile_bucket_tags.inc"
#include "test_utils.h"
#include <fmt/ostream.h>

namespace {
using namespace spectator;

template <class T>
std::unique_ptr<T> getTimer(Registry* r) {
  auto id = Id::Of("t");
  return std::make_unique<T>(r, id, absl::ZeroDuration(), absl::Seconds(100));
}

using Implementations = testing::Types<PercentileTimer>;

// satisfy -Wgnu-zero-variadic-macro-arguments in TYPED_TEST_SUITE
class NameGenerator {
 public:
  template <typename T>
  static std::string GetName(int) {
    if constexpr (std::is_same_v<T, PercentileTimer>) return "PercentileTimer";
    return "unknownType";
  }
};

template <class T>
class PercentileTimerTest : public ::testing::Test {
 protected:
  PercentileTimerTest()
      : r{GetConfiguration(), spectatord::Logger()},
        timer{getTimer<T>(&r)},
        restricted_timer{&r, Id::Of("t2"), absl::Milliseconds(5), absl::Seconds(2)} {}
  Registry r;
  std::unique_ptr<T> timer;
  T restricted_timer;
};

TYPED_TEST_SUITE(PercentileTimerTest, Implementations, NameGenerator);

TYPED_TEST(PercentileTimerTest, Percentile) {
  for (auto i = 0; i < 100000; ++i) {
    this->timer->Record(absl::Milliseconds(i));
  }

  for (auto i = 0; i <= 100; ++i) {
    auto expected = static_cast<double>(i);
    auto threshold = 0.15 * expected;
    EXPECT_NEAR(expected, this->timer->Percentile(i), threshold);
  }
}

TYPED_TEST(PercentileTimerTest, Measure) {
  auto elapsed = absl::Milliseconds(42);
  this->timer->Record(elapsed);

  auto elapsed_nanos = absl::ToInt64Nanoseconds(elapsed);

  auto ms = this->r.Measurements();
  auto actual = measurements_to_map(ms);
  auto expected = std::map<std::string, double>{};

  auto percentileTag = kTimerTags.at(PercentileBucketIndexOf(elapsed_nanos));
  expected[fmt::format("t|percentile={}|statistic=percentile", percentileTag)] = 1;

  expected["t|statistic=count"] = 1;
  auto elapsed_secs = elapsed_nanos / 1e9;
  expected["t|statistic=max"] = elapsed_secs;
  expected["t|statistic=totalTime"] = elapsed_secs;
  expected["t|statistic=totalOfSquares"] = elapsed_secs * elapsed_secs;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first]) << expected_m.first;
  }
}

TYPED_TEST(PercentileTimerTest, CountTotal) {
  for (auto i = 0; i < 100; ++i) {
    this->timer->Record(absl::Nanoseconds(i));
  }

  EXPECT_EQ(this->timer->Count(), 100);
  EXPECT_EQ(this->timer->TotalTime(), 100 * 99 / 2);  // sum(1,n) = n * (n - 1) / 2
}

TYPED_TEST(PercentileTimerTest, Restrict) {
  auto& t = this->restricted_timer;

  t.Record(absl::Milliseconds(1));
  t.Record(absl::Seconds(10));
  t.Record(absl::Milliseconds(10));

  // not restricted here
  auto total = absl::Milliseconds(1) + absl::Seconds(10) + absl::Milliseconds(10);
  EXPECT_EQ(t.TotalTime(), absl::ToInt64Nanoseconds(total));
  EXPECT_EQ(t.Count(), 3);

  auto measurements = this->r.Measurements();
  auto actual = measurements_to_map(measurements);
  auto expected = std::map<std::string, double>{};

  // 5ms
  auto minPercTag = kTimerTags.at(PercentileBucketIndexOf(5l * 1000 * 1000));
  // 2s
  auto maxPercTag = kTimerTags.at(PercentileBucketIndexOf(2l * 1000 * 1000 * 1000));
  // 10ms --> not restricted
  auto percTag = kTimerTags.at(PercentileBucketIndexOf(10l * 1000 * 1000));
  std::string name = t.MeterId().Name().Get();

  expected[fmt::format("{}|percentile={}|statistic=percentile", name, minPercTag)] = 1;
  expected[fmt::format("{}|percentile={}|statistic=percentile", name, maxPercTag)] = 1;
  expected[fmt::format("{}|percentile={}|statistic=percentile", name, percTag)] = 1;

  expected[fmt::format("{}|statistic=count", name)] = 3;
  auto elapsed_secs = absl::ToDoubleSeconds(total);
  auto totalSq = (1e6 * 1e6 + 10e9 * 10e9 + 10e6 * 10e6) / 1e18;
  expected[fmt::format("{}|statistic=max", name)] = 10;
  expected[fmt::format("{}|statistic=totalTime", name)] = elapsed_secs;
  expected[fmt::format("{}|statistic=totalOfSquares", name)] = totalSq;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first]) << expected_m.first;
  }
}
}  // namespace
