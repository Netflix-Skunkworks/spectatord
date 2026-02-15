#include "common_refs.h"
#include "monotonic_counter.h"
#include <gtest/gtest.h>

namespace
{

using spectator::Id;
using spectator::refs;

auto getMonotonicCounter(std::string_view name) -> spectator::MonotonicCounter
{
	return spectator::MonotonicCounter(Id::Of(name));
}

TEST(MonotonicCounter, Init)
{
	auto c = getMonotonicCounter("foo");
	EXPECT_TRUE(std::isnan(c.Delta()));

	c.Set(42);
	EXPECT_TRUE(std::isnan(c.Delta()));

	spectator::Measurements ms;
	c.Measure(&ms);
	ASSERT_TRUE(ms.empty());

	c.Set(84);
	EXPECT_DOUBLE_EQ(c.Delta(), 42.0);
}

TEST(MonotonicCounter, NegativeDelta)
{
	auto c = getMonotonicCounter("neg");
	EXPECT_TRUE(std::isnan(c.Delta()));

	spectator::Measurements ms;
	c.Set(100.0);
	c.Measure(&ms);
	EXPECT_TRUE(ms.empty());

	c.Set(99.0);
	c.Measure(&ms);
	EXPECT_TRUE(ms.empty());

	c.Set(98.0);
	c.Measure(&ms);
	EXPECT_TRUE(ms.empty());

	c.Set(100.0);
	c.Measure(&ms);
	EXPECT_EQ(ms.size(), 1);
	auto id = c.MeterId().WithStat(refs().count());
	auto expected = spectator::Measurement{id, 2.0};
	EXPECT_EQ(expected, ms.front());
}

TEST(MonotonicCounter, Id)
{
	auto c = getMonotonicCounter("id");
	auto id = Id("id", spectator::Tags{});
	EXPECT_EQ(c.MeterId(), id);
}

TEST(MonotonicCounter, Measure)
{
	auto c = getMonotonicCounter("measure");
	c.Set(42);
	spectator::Measurements measures;
	c.Measure(&measures);
	ASSERT_TRUE(measures.empty());  // initialize

	EXPECT_DOUBLE_EQ(c.Delta(), 0.0) << "MonotonicCounters should reset their value after being measured";

	c.Set(84.0);
	c.Measure(&measures);
	auto id = c.MeterId().WithStat(refs().count());
	std::vector<spectator::Measurement> expected({{id, 42.0}});
	EXPECT_EQ(expected, measures);

	measures.clear();
	c.Measure(&measures);
	EXPECT_TRUE(measures.empty()) << "MonotonicCounters should not report delta=0";
}

TEST(MonotonicCounter, DefaultStatistic)
{
	spectator::Measurements measures;
	auto c = spectator::MonotonicCounter(Id::Of("foo", {{"statistic", "totalAmount"}}));
	c.Set(42);
	c.Measure(&measures);
	c.Set(84.0);
	c.Measure(&measures);
	auto id = Id::Of("foo", {{"statistic", "totalAmount"}});
	std::vector<spectator::Measurement> expected({{id, 42.0}});
	EXPECT_EQ(expected, measures);
}

TEST(MonotonicCounter, Update)
{
	auto counter = getMonotonicCounter("m");
	counter.Set(1);

	auto t1 = counter.Updated();
	auto t2 = counter.Updated();
	EXPECT_EQ(t1, t2);
	usleep(1);
	counter.Set(2);
	EXPECT_TRUE(counter.Updated() > t1);
}
}  // namespace
