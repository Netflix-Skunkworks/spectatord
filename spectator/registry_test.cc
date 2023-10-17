#include "registry.h"
#include "test_utils.h"
#include <fmt/ostream.h>
#include <gtest/gtest.h>

namespace {
using spectator::GetConfiguration;
using spectator::Id;
using spectator::Registry;
using spectator::Tags;
using spectatord::Logger;

TEST(Registry, AgeGauge) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto g = r.GetAgeGauge("foo");
  g->UpdateLastSuccess();
  // apple silicon is so fast that we need to sleep here to allow it to accumulate time
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_TRUE(g->Value() > 0.0);
}

TEST(Registry, Counter) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto c = r.GetCounter("foo");
  c->Increment();
  EXPECT_EQ(c->Count(), 1);
}

TEST(Registry, CounterGet) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto c = r.GetCounter("foo");
  c->Increment();
  EXPECT_EQ(r.GetCounter("foo")->Count(), 1);
}

TEST(Registry, DistSummary) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto ds = r.GetDistributionSummary("ds");
  ds->Record(100);
  EXPECT_EQ(r.GetDistributionSummary("ds")->TotalAmount(), 100);
}

TEST(Registry, Gauge) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto g = r.GetGauge("g");
  auto g2 = r.GetGauge("g", Tags{{"id", "2"}});
  g->Set(100);
  g2->Set(101);
  EXPECT_DOUBLE_EQ(r.GetGauge("g")->Get(), 100);
  EXPECT_DOUBLE_EQ(r.GetGauge("g", Tags{{"id", "2"}})->Get(), 101);
}

TEST(Registry, MaxGauge) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto g = r.GetMaxGauge("g");
  auto g2 = r.GetMaxGauge("g", Tags{{"id", "2"}});
  g->Update(100);
  g2->Set(101);
  EXPECT_DOUBLE_EQ(r.GetMaxGauge("g")->Get(), 100);
  EXPECT_DOUBLE_EQ(r.GetMaxGauge("g", Tags{{"id", "2"}})->Get(), 101);
}

TEST(Registry, MonotonicCounter) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto c = r.GetMonotonicCounter("m");
  auto c2 = r.GetMonotonicCounter("m", Tags{{"id", "2"}});
  spectator::Measurements ms;
  c->Set(100);
  c->Measure(&ms);
  c->Set(200);
  c2->Set(100);
  c2->Measure(&ms);
  c2->Set(201);
  EXPECT_DOUBLE_EQ(r.GetMonotonicCounter("m")->Delta(), 100);
  EXPECT_DOUBLE_EQ(r.GetMonotonicCounter("m", Tags{{"id", "2"}})->Delta(), 101);
}

TEST(Registry, Timer) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto t = r.GetTimer("t");
  auto t2 = r.GetTimer("t", Tags{{"id", "2"}});
  t->Record(std::chrono::microseconds(1));
  t2->Record(std::chrono::microseconds(2));
  EXPECT_EQ(r.GetTimer("t")->TotalTime(), 1000);
  EXPECT_EQ(r.GetTimer("t", Tags{{"id", "2"}})->TotalTime(), 2000);
}

TEST(Registry, Meters) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto t = r.GetTimer("t");
  auto c = r.GetCounter("c");
  r.GetTimer("t")->Count();

  auto timers = my_timers(r);
  ASSERT_EQ(timers.size(), 1);
  auto counters = my_counters(r);
  ASSERT_EQ(counters.size(), 1);
}

TEST(Registry, MeasurementTest) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto c = r.GetCounter("c", Tags{{"id", "up"}});
  c->Increment();
  spectator::Measurements ms;
  c->Measure(&ms);
  auto m = ms.front();
  // test to string
  auto str = fmt::format("{}", m);
  EXPECT_EQ(str, "Measurement{Id(c, [id->up, statistic->count]), 1}");

  // test equals
  c->Increment();
  ms.clear();
  c->Measure(&ms);
  auto m2 = ms.front();
  EXPECT_EQ(m, m2);
}

struct ExpRegistry : public Registry {
  explicit ExpRegistry(std::unique_ptr<spectator::Config> cfg)
      : Registry(std::move(cfg), spectatord::Logger()) {}

  void expire() { remove_expired_meters(); }
};

TEST(Registry, Expiration) {
  auto cfg = GetConfiguration();
  cfg->expiration_frequency = absl::Milliseconds(1);
  cfg->meter_ttl = absl::Milliseconds(1);
  ExpRegistry r{std::move(cfg)};

  auto c = r.GetCounter("c");
  auto g = r.GetGauge("g");
  auto d = r.GetDistributionSummary("d");
  auto t = r.GetTimer("t");
  auto m = r.GetMaxGauge("m");

  auto my_size = my_meters_size(r);
  ASSERT_EQ(my_size, 5);

  usleep(2000);  // 2ms
  c->Increment();
  // gauge is tricky because we enforce a min 5s TTL
  g->Set(1.0);
  r.expire();

  ASSERT_EQ(my_meters_size(r), 2);
}

TEST(Registry, Size) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto base_number = r.Size();

  r.GetCounter("foo");
  r.GetTimer("bar");
  EXPECT_EQ(r.Size(), 2 + base_number);

  r.GetCounter("foo");
  r.GetTimer("bar2");
  EXPECT_EQ(r.Size(), 3 + base_number);
}

TEST(Registry, OnMeasurements) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  auto called = false;
  auto found = false;
  auto cb = [&](const std::vector<spectator::Measurement>& ms) {
    called = true;
    auto id = Id::Of("some.counter", {{"statistic", "count"}});
    auto expected = spectator::Measurement{id, 1.0};
    auto it = std::find(ms.begin(), ms.end(), expected);
    found = it != ms.end();
  };
  r.OnMeasurements(cb);
  r.GetCounter("some.counter")->Increment();
  r.Measurements();
  ASSERT_TRUE(called);
  ASSERT_TRUE(found);
}

TEST(Registry, DistSummary_Size) {
  Registry r{GetConfiguration(), spectatord::Logger()};
  r.GetTimer("foo")->Record(std::chrono::seconds{42});
  r.GetCounter("bar")->Increment();
  // we have 4 measurements from the timer + 1 from the counter
  r.Measurements();
  EXPECT_DOUBLE_EQ(r.GetDistributionSummary("spectator.registrySize")->TotalAmount(), 5.0);
}

TEST(Registry, DeleteMeters) {
  Registry r{GetConfiguration(), spectatord::Logger()};

  r.GetGauge("g1")->Set(1);
  r.GetGauge("g2")->Set(1);
  r.GetGauge("g3")->Set(1);
  EXPECT_DOUBLE_EQ(r.Gauges().size(), 3.0);

  auto succeeded = r.DeleteMeter("g", Id {"g1", Tags{}});
  auto failed = r.DeleteMeter("g", Id {"g1", Tags{}});
  EXPECT_TRUE(succeeded);
  EXPECT_FALSE(failed);
  EXPECT_DOUBLE_EQ(r.Gauges().size(), 2.0);

  r.DeleteAllMeters("g");
  EXPECT_DOUBLE_EQ(r.Gauges().size(), 0.0);

  r.GetAgeGauge("ag1")->UpdateLastSuccess();
  r.GetAgeGauge("ag2")->UpdateLastSuccess();
  r.GetAgeGauge("ag3")->UpdateLastSuccess();
  EXPECT_DOUBLE_EQ(r.AgeGauges().size(), 3.0);

  succeeded = r.DeleteMeter("A", Id {"ag1", Tags{}});
  failed = r.DeleteMeter("A", Id {"ag1", Tags{}});
  EXPECT_TRUE(succeeded);
  EXPECT_FALSE(failed);
  EXPECT_DOUBLE_EQ(r.AgeGauges().size(), 2.0);

  r.DeleteAllMeters("A");
  EXPECT_DOUBLE_EQ(r.AgeGauges().size(), 0.0);
}

}  // namespace
