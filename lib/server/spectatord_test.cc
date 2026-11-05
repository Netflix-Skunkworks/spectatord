#include "spectator/registry/test_utils.h"
#include "gtest/gtest.h"
#include "server/transport/local.h"
#include "spectatord.h"
#include "spectator/strings/string_pool.h"

#include <fmt/ostream.h>

namespace
{

using spectatord::get_measurement;
using spectatord::Logger;

std::unique_ptr<spectator::Config> GetConfiguration() { return std::make_unique<spectator::Config>(); }

std::string id_to_string(const spectator::Id& id)
{
	std::string res = id.Name().Get();
	const auto& tags = id.GetTags();
	std::map<std::string, std::string> sorted_tags;
	for (const auto& kv : tags)
	{
		sorted_tags[kv.key.Get()] = kv.value.Get();
	}

	for (const auto& pair : sorted_tags)
	{
		res += fmt::format("|{}={}", pair.first, pair.second);
	}
	return res;
}

std::map<std::string, double> measurements_to_map(const std::vector<spectator::Measurement>& measurements)
{
	auto res = std::map<std::string, double>();
	for (const auto& m : measurements)
	{
		res[id_to_string(m.id)] = m.value;
	}
	return res;
}

class test_server : public spectatord::Server
{
   public:
	explicit test_server(spectator::Registry* registry)
	    : Server(false, 0, 0, spectatord::kSocketNameDgram, registry), registry_{registry}
	{
	}
	std::optional<std::string> parse_msg(char* msg) { return parse(msg); }
	spectatord::id_cache::stats cache_stats() const { return id_cache_stats(); }
	bool clear_cache_if_saturated() { return id_cache_clear_if_saturated(); }
	void clear_cache_for_test() { id_cache_clear_for_test(); }
	std::optional<std::string> test_parse_statsd(char* buffer) { return parse_statsd(buffer); }

	std::map<std::string, double> measurements()
	{
		auto ms = registry_->Measurements();
		return measurements_to_map(ms);
	}

   private:
	spectator::Registry* registry_;
};

struct free_deleter
{
	template <typename T>
	void operator()(T* p) const
	{
		std::free(const_cast<std::remove_const_t<T>*>(p));
	}
};
using char_ptr = std::unique_ptr<char, free_deleter>;

void test_statsd(char* buffer, const std::map<std::string, double>& expected)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	server.test_parse_statsd(buffer);
	auto actual = server.measurements();

	ASSERT_EQ(actual.size(), expected.size());
	for (const auto& id_value : expected)
	{
		auto& id = id_value.first;
		auto it = actual.find(id);
		if (it != actual.end())
		{
			auto actual_value = it->second;
			EXPECT_DOUBLE_EQ(id_value.second, actual_value) << "Wrong value for " << id;
		}
		else
		{
			ADD_FAILURE() << "Unable to find " << id;
		}
	}
}

TEST(Spectatord, Statsd_Multiline)
{
	// end without a newline
	char_ptr ctr{strdup("page.views:1|c\npage.views:3|c\npage.views:5|c")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 3},
	                                       {"page.views|statistic=count", 9.0}};
	test_statsd(ctr.get(), expected);

	// end with a newline
	ctr.reset(strdup("page.views:1|c\npage.views:3|c\npage.views:5|c\n"));
	test_statsd(ctr.get(), expected);

	// empty lines
	ctr.reset(strdup("page.views:1|c\n\npage.views:3|c\n\npage.views:5|c\n\n"));
	test_statsd(ctr.get(), expected);
}

TEST(Spectatord, Statsd_MultilineWithErrors)
{
	char_ptr ctr{strdup("page.views:1|c\nbad\npage.views:3|c")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 2},
	                                       {"spectatord.parseErrors|statistic=count", 1},
	                                       {"page.views|statistic=count", 4.0}};
	test_statsd(ctr.get(), expected);
}

TEST(Spectatord, Statsd_Counter)
{
	char_ptr ctr{strdup("page.views:1|c\n")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"page.views|statistic=count", 1.0}};
	test_statsd(ctr.get(), expected);
}

TEST(Spectatord, Statsd_CounterTags)
{
	char_ptr c{strdup("users.online:10|c|#country:china")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"users.online|country=china|statistic=count", 10}};
	test_statsd(c.get(), expected);
}

TEST(Spectatord, Statsd_GaugeNoTags)
{
	char_ptr g{strdup("fuel.level:0.5|g")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"fuel.level|statistic=gauge", 0.5}};
	test_statsd(g.get(), expected);
}

TEST(Spectatord, Statsd_GaugeDefaultTags)
{
	char_ptr g{strdup("custom.metric:60|g|#shell")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"custom.metric|shell=1|statistic=gauge", 60.0}};
	test_statsd(g.get(), expected);
}

TEST(Spectatord, Statsd_HistogramAsDistSummary)
{
	char_ptr h{strdup("song.length:240|h|#region:east")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"song.length|region=east|statistic=count", 1},
	                                       {"song.length|region=east|statistic=totalAmount", 240},
	                                       {"song.length|region=east|statistic=max", 240},
	                                       {"song.length|region=east|statistic=totalOfSquares", 240 * 240}};
	test_statsd(h.get(), expected);
}

TEST(Spectatord, Statsd_SampledHistogram)
{
	char_ptr h{strdup("song.length:240|h|@0.5")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"song.length|statistic=count", 2},
	                                       {"song.length|statistic=totalAmount", 480},
	                                       {"song.length|statistic=totalOfSquares", 240 * 240 * 2},
	                                       {"song.length|statistic=max", 240}};
	test_statsd(h.get(), expected);
}

TEST(Spectatord, DISABLED_Statsd_SetCardinality)
{
	char_ptr s{strdup("users.uniques:1234|s")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"users.uniques|statistic=gauge", 1}};
	test_statsd(s.get(), expected);
}

TEST(Spectatord, Statsd_Timer)
{
	char_ptr timer{strdup("req.latency:350|ms|#country:us")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"req.latency|country=us|statistic=count", 1},
	                                       {"req.latency|country=us|statistic=totalTime", 0.35},
	                                       {"req.latency|country=us|statistic=totalOfSquares", 0.35 * 0.35},
	                                       {"req.latency|country=us|statistic=max", 0.35}};
	test_statsd(timer.get(), expected);
}

TEST(Spectatord, Statsd_SampledTimer)
{
	char_ptr timer{strdup("req.latency:350|ms|@0.1|#country:us")};
	std::map<std::string, double> expected{{"spectatord.parsedCount|statistic=count", 1},
	                                       {"req.latency|country=us|statistic=count", 10},
	                                       {"req.latency|country=us|statistic=totalOfSquares", (0.35 * 0.35) * 10},
	                                       {"req.latency|country=us|statistic=totalTime", 3.5},
	                                       {"req.latency|country=us|statistic=max", 0.35}};
	test_statsd(timer.get(), expected);
}

TEST(Spectatord, ParseNoTags)
{
	auto logger = Logger();
	char_ptr line{strdup("n:1")};
	std::string err_msg;
	auto measurement = *get_measurement('c', line.get(), &err_msg);
	logger->info("Got {} = {}", measurement.id, measurement.value.d);
	EXPECT_DOUBLE_EQ(measurement.value.d, 1.0);
	EXPECT_EQ(measurement.id, spectator::Id("n", spectator::Tags{}));
}

TEST(Spectatord, ParseOneTag)
{
	auto logger = Logger();
	char_ptr line{strdup("my.name,foo=bar:42.0")};
	std::string err_msg;
	auto measurement = *(get_measurement('c', line.get(), &err_msg));
	logger->info("Got {} = {}", measurement.id, measurement.value.d);
	EXPECT_DOUBLE_EQ(measurement.value.d, 42.0);
	EXPECT_EQ(measurement.id, spectator::Id("my.name", spectator::Tags{{"foo", "bar"}}));
	EXPECT_TRUE(err_msg.empty());
}

TEST(Spectatord, ParseMultipleTags)
{
	auto logger = Logger();
	char_ptr line{strdup("n,foo=bar,k=v1,k2=v2:2")};
	std::string err_msg;
	auto measurement = *get_measurement('c', line.get(), &err_msg);
	logger->info("Got {} = {}", measurement.id, measurement.value.d);
	EXPECT_DOUBLE_EQ(measurement.value.d, 2.0);
	EXPECT_EQ(measurement.id, spectator::Id("n", spectator::Tags{{"foo", "bar"}, {"k", "v1"}, {"k2", "v2"}}));
	EXPECT_TRUE(err_msg.empty());
}

TEST(Spectatord, ParseMissingName)
{
	auto logger = Logger();
	char_ptr line{strdup(":1")};
	std::string err_msg;
	auto measurement = get_measurement('c', line.get(), &err_msg);
	EXPECT_FALSE(measurement);
	logger->info("Got err_msg = {}", err_msg);
	EXPECT_FALSE(err_msg.empty());
}

TEST(Spectatord, ParseMissingValue)
{
	auto logger = Logger();
	char_ptr line{strdup("name:")};
	std::string err_msg;
	auto measurement = get_measurement('c', line.get(), &err_msg);
	logger->info("Got err_msg = {}", err_msg);
	EXPECT_FALSE(measurement);
	EXPECT_FALSE(err_msg.empty());
}

TEST(Spectatord, ParseIgnoringStuffAtTheEnd)
{
	auto logger = Logger();
	char_ptr line{strdup("name:123abc")};
	std::string err_msg;
	auto measurement = *get_measurement('c', line.get(), &err_msg);
	EXPECT_DOUBLE_EQ(measurement.value.d, 123.0);
	EXPECT_EQ(measurement.id, spectator::Id("name", spectator::Tags{}));
	logger->info("Got err_msg = {}", err_msg);
	EXPECT_FALSE(err_msg.empty());
}

TEST(Spectatord, ParseMultiline)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line{strdup("c:counter.name:10\nc:counter.name:20\nc:counter.name:12")};
	server.parse_msg(line.get());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 3);
	EXPECT_DOUBLE_EQ(map["counter.name|statistic=count"], 42);
}

TEST(Spectatord, ParseAgeGauge)
{
	auto logger = Logger();
	auto cfg = GetConfiguration();
	cfg->age_gauge_limit = 1;
	spectator::Registry registry{std::move(cfg), logger};
	test_server server{&registry};

	char_ptr line{strdup("A:gauge.name:0")};
	server.parse_msg(line.get());
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	auto map = server.measurements();
	EXPECT_EQ(my_age_gauges(registry).size(), 1);
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	auto secs = map["gauge.name|statistic=gauge"];
	EXPECT_TRUE(secs >= 1e-3 && secs < 1);

	auto now_secs = absl::GetCurrentTimeNanos() / 1e9 - 1;
	auto explicit_t = fmt::format("A:gauge.name:{}", now_secs);
	char_ptr line2{strdup(explicit_t.c_str())};
	server.parse_msg(line2.get());
	map = server.measurements();
	EXPECT_EQ(my_age_gauges(registry).size(), 1);
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	secs = map["gauge.name|statistic=gauge"];
	EXPECT_TRUE(secs >= 1 && secs < 2);
}

TEST(Spectatord, TooManyAgeGauges)
{
	auto logger = Logger();
	auto cfg = GetConfiguration();
	cfg->age_gauge_limit = 1;
	spectator::Registry registry{std::move(cfg), logger};
	test_server server{&registry};

	char_ptr line{strdup("A:gauge.name,app=spectatord,region=us-east-1:0")};
	server.parse_msg(line.get());
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	auto map = server.measurements();
	EXPECT_EQ(my_age_gauges(registry).size(), 1);
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);

	// add a second gauge, against a limit of 1, to see the list of known gauges
	char_ptr line2{strdup("A:gauge.second_name:0")};
	server.parse_msg(line2.get());
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	map = server.measurements();
	EXPECT_EQ(my_age_gauges(registry).size(), 1);
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
}

TEST(Spectatord, ParseCounter)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line1{strdup("c:counter.name:10")};
	char_ptr line2{strdup("c:counter.name:10")};
	server.parse_msg(line1.get());
	server.parse_msg(line2.get());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 2);
	EXPECT_DOUBLE_EQ(map["counter.name|statistic=count"], 20);
}

TEST(Spectatord, ParseDistSummary)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line{strdup("d:dist.summary:1")};
	server.parse_msg(line.get());

	char_ptr line2{strdup("d:dist.summary:2")};
	server.parse_msg(line2.get());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 2);
	EXPECT_DOUBLE_EQ(map["dist.summary|statistic=count"], 2);
	EXPECT_DOUBLE_EQ(map["dist.summary|statistic=totalAmount"], 3);
	EXPECT_DOUBLE_EQ(map["dist.summary|statistic=max"], 2);
}

TEST(Spectatord, ParseGauge)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line{strdup("g:gauge.name:1.234")};
	server.parse_msg(line.get());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	EXPECT_DOUBLE_EQ(map["gauge.name|statistic=gauge"], 1.234);
}

TEST(Spectatord, ParseGaugeTtl)
{
	auto logger = Logger();
	auto cfg = GetConfiguration();
	cfg->meter_ttl = absl::Minutes(15);
	spectator::Registry registry{std::move(cfg), logger};
	test_server server{&registry};

	char_ptr line{strdup("g,5:gauge.name:1.234")};
	server.parse_msg(line.get());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	EXPECT_DOUBLE_EQ(map["gauge.name|statistic=gauge"], 1.234);

	// honors the Ttl
	EXPECT_EQ(registry.GetGauge("gauge.name")->GetTtl(), absl::Seconds(5));

	char_ptr line2{strdup("g,15:gauge.name:1.234")};
	server.parse_msg(line2.get());
	// overrides the Ttl
	EXPECT_EQ(registry.GetGauge("gauge.name")->GetTtl(), absl::Seconds(15));

	char_ptr line3{strdup("g:gauge.name:1.234")};
	server.parse_msg(line3.get());
	// preserves the previous Ttl
	EXPECT_EQ(registry.GetGauge("gauge.name")->GetTtl(), absl::Seconds(15));
}

TEST(Spectatord, ParseCacheHit)
{
	// the second and third lines for the same id resolve through the meter
	// cache, skipping tag parsing and the registry lookup
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line1{strdup("c:counter.name,id=1,foo=bar:10")};
	char_ptr line2{strdup("c:counter.name,id=1,foo=bar:5")};
	char_ptr line3{strdup("t:timer.name,id=1,foo=bar:0.5")};
	EXPECT_FALSE(server.parse_msg(line1.get()).has_value());
	EXPECT_FALSE(server.parse_msg(line2.get()).has_value());
	// same id string, different line type: a different meter
	EXPECT_FALSE(server.parse_msg(line3.get()).has_value());

	// without this the cache could discard every hit and still pass
	const auto stats = server.cache_stats();
	EXPECT_GT(stats.hits, 0u) << "no line was served from the cache";
	EXPECT_EQ(stats.size, 2u) << "expected one entry per (type, id)";

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 3);
	EXPECT_DOUBLE_EQ(map["counter.name|foo=bar|id=1|statistic=count"], 15);
	EXPECT_DOUBLE_EQ(map["timer.name|foo=bar|id=1|statistic=count"], 1);
}
TEST(Spectatord, ParseCacheIgnoresLinesWithoutValueSeparator)
{
	// "S:a,z" has no value separator at all, yet it parses: a distinct count
	// sketch accepts an empty value. Its id must not end up cached under an
	// empty key, because a later line whose id is empty ("S::...") would then
	// resolve to that meter and silently record into the wrong metric.
	auto logger = Logger();
	spectator::Registry clean_registry{GetConfiguration(), logger};
	spectator::Registry poked_registry{GetConfiguration(), logger};
	test_server clean{&clean_registry};
	test_server poked{&poked_registry};

	char_ptr a1{strdup("S:a,z")};
	char_ptr a2{strdup("S:a,z")};
	EXPECT_FALSE(clean.parse_msg(a1.get()).has_value());
	EXPECT_FALSE(poked.parse_msg(a2.get()).has_value());

	// an empty id must be rejected, exactly as it is without the cache
	char_ptr empty_id{strdup("S::YWJj")};
	auto err = poked.parse_msg(empty_id.get());
	ASSERT_TRUE(err.has_value()) << "an empty id was accepted";
	EXPECT_EQ(*err, "Missing name");

	// measurements() resets the meters, so each registry is read exactly once
	auto expected = clean.measurements();
	auto actual = poked.measurements();
	expected.erase("spectatord.parseErrors|statistic=count");
	actual.erase("spectatord.parseErrors|statistic=count");
	EXPECT_EQ(actual, expected) << "the rejected line still changed a meter";
}

TEST(Spectatord, ParseCacheHandlesColonInTagKey)
{
	// A tag key is located with find('='), which does not stop at a colon, so a
	// tag key may contain ':' and the first ':' in a line is then not the end of
	// the id. Caching keyed on "text up to the first ':'" would let a shorter
	// line sharing that prefix resolve to the wrong meter.
	auto logger = Logger();
	auto plain_cfg = GetConfiguration();
	plain_cfg->meter_cache_size = 0;  // the comparison is meaningless otherwise
	spectator::Registry cached_registry{GetConfiguration(), logger};
	spectator::Registry plain_registry{std::move(plain_cfg), logger};
	test_server cached{&cached_registry};
	test_server plain{&plain_registry};

	// the real id here is "m,a:b=1"; only "m,a" precedes the first ':'
	for (const auto* line : {"c:m,a:b=1:5", "c:m,a:9"})
	{
		char_ptr a{strdup(line)};
		char_ptr b{strdup(line)};
		auto err_cached = cached.parse_msg(a.get());
		auto err_plain = plain.parse_msg(b.get());
		EXPECT_EQ(err_cached, err_plain) << "divergent result for '" << line << "'";
	}

	EXPECT_EQ(cached.measurements(), plain.measurements()) << "a line was recorded against the wrong meter";
}

TEST(Spectatord, ParseCacheCapacityLimit)
{
	// Once the cache is full, further ids still parse correctly through the
	// uncached path; this is what bounds memory when cardinality is unbounded.
	auto logger = Logger();
	auto cfg = GetConfiguration();
	cfg->meter_cache_size = 1;
	spectator::Registry registry{std::move(cfg), logger};
	test_server server{&registry};

	for (auto i = 0; i < 3; ++i)
	{
		char_ptr first{strdup(fmt::format("c:counter{}:10", i).c_str())};
		char_ptr second{strdup(fmt::format("c:counter{}:5", i).c_str())};
		EXPECT_FALSE(server.parse_msg(first.get()).has_value());
		EXPECT_FALSE(server.parse_msg(second.get()).has_value());
	}

	auto map = server.measurements();
	for (auto i = 0; i < 3; ++i)
	{
		EXPECT_DOUBLE_EQ(map[fmt::format("counter{}|statistic=count", i)], 15) << "wrong value for counter" << i;
	}

	// the bound itself, otherwise this test passes with no limit at all
	const auto stats = server.cache_stats();
	EXPECT_EQ(stats.size, 1u) << "the cache grew past its capacity";
	EXPECT_GT(stats.rejected, 0u) << "nothing was rejected, so the limit never engaged";
}

TEST(Spectatord, ParseCacheRejectsOversizedIds)
{
	// Nothing bounds the length of a name or tag value, so a few very long ids
	// must not be able to consume the whole cache budget. They still parse.
	auto logger = Logger();
	auto cfg = GetConfiguration();
	cfg->meter_cache_size = 4;  // budget is 4 * 512 bytes of key data
	spectator::Registry registry{std::move(cfg), logger};
	test_server server{&registry};

	const auto huge_name = std::string(4096, 'x');
	for (auto i = 0; i < 2; ++i)
	{
		char_ptr line{strdup(fmt::format("c:{}:7", huge_name).c_str())};
		EXPECT_FALSE(server.parse_msg(line.get()).has_value());
	}

	// a normal id still fits and still works alongside it
	char_ptr small1{strdup("c:small.counter:3")};
	char_ptr small2{strdup("c:small.counter:4")};
	EXPECT_FALSE(server.parse_msg(small1.get()).has_value());
	EXPECT_FALSE(server.parse_msg(small2.get()).has_value());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map[fmt::format("{}|statistic=count", huge_name)], 14);
	EXPECT_DOUBLE_EQ(map["small.counter|statistic=count"], 7);

	// ids that each fit the budget must still be rejected once their sum
	// exceeds it, otherwise the limit is per entry rather than cumulative
	for (auto i = 0; i < 4; ++i)
	{
		char_ptr medium{strdup(fmt::format("c:{}{}:1", std::string(400, 'm'), i).c_str())};
		EXPECT_FALSE(server.parse_msg(medium.get()).has_value());
	}

	// a 4096 byte id must not fit a 4 * 512 byte budget
	const auto stats = server.cache_stats();
	EXPECT_LE(stats.bytes, stats.capacity * spectatord::id_cache::kBytesPerEntry)
	    << "the byte budget is not cumulative";
	EXPECT_GT(stats.rejected, 0u) << "the oversized id was cached anyway";
}

TEST(Spectatord, ParseCacheDisabled)
{
	// meter_cache_size = 0 turns the cache off completely
	auto logger = Logger();
	auto cfg = GetConfiguration();
	cfg->meter_cache_size = 0;
	spectator::Registry registry{std::move(cfg), logger};
	test_server server{&registry};

	char_ptr line1{strdup("c:counter.name,app=foo:10")};
	char_ptr line2{strdup("c:counter.name,app=foo:5")};
	char_ptr line3{strdup("g,5:gauge.name:1.5")};
	EXPECT_FALSE(server.parse_msg(line1.get()).has_value());
	EXPECT_FALSE(server.parse_msg(line2.get()).has_value());
	EXPECT_FALSE(server.parse_msg(line3.get()).has_value());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["counter.name|app=foo|statistic=count"], 15);
	EXPECT_DOUBLE_EQ(map["gauge.name|statistic=gauge"], 1.5);
	EXPECT_EQ(registry.GetGauge("gauge.name")->GetTtl(), absl::Seconds(5));
}

TEST(Spectatord, ParseCacheInvalidatedByMeterDeletion)
{
	// deleting a meter from the registry bumps the expiration epoch and
	// invalidates the cache, so the next line re-resolves through the
	// registry instead of writing to the orphaned meter
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line1{strdup("g:gauge.name:1.0")};
	EXPECT_FALSE(server.parse_msg(line1.get()).has_value());
	EXPECT_TRUE(registry.DeleteMeter("g", spectator::Id::Of("gauge.name")));

	char_ptr line2{strdup("g:gauge.name:2.0")};
	EXPECT_FALSE(server.parse_msg(line2.get()).has_value());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["gauge.name|statistic=gauge"], 2.0);
}

TEST(Spectatord, ParseCacheHitInvalidValue)
{
	// a bad value for a cached id is still rejected, and the cached meter is
	// not updated
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line1{strdup("c:counter.name:10")};
	char_ptr line2{strdup("c:counter.name:garbage")};
	EXPECT_FALSE(server.parse_msg(line1.get()).has_value());
	EXPECT_TRUE(server.parse_msg(line2.get()).has_value());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["counter.name|statistic=count"], 10);
	EXPECT_DOUBLE_EQ(map["spectatord.parseErrors|statistic=count"], 1);
}

TEST(Spectatord, ParseMonoCounter)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line1{strdup("C:mono_counter.name:10.5")};
	server.parse_msg(line1.get());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	EXPECT_DOUBLE_EQ(map["mono_counter.name|statistic=count"], 0);

	char_ptr line2{strdup("C:mono_counter.name:20.75")};
	server.parse_msg(line2.get());

	map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	EXPECT_DOUBLE_EQ(map["mono_counter.name|statistic=count"], 10.25);
}

TEST(Spectatord, ParseMonoCounterUint)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line1{strdup("U:mono_counter_uint.name:10")};
	server.parse_msg(line1.get());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	EXPECT_DOUBLE_EQ(map["mono_counter_uint.name|statistic=count"], 0);

	char_ptr line2{strdup("U:mono_counter_uint.name:20")};
	server.parse_msg(line2.get());

	map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	EXPECT_DOUBLE_EQ(map["mono_counter_uint.name|statistic=count"], 10);

	// fractional numbers are truncated
	char_ptr line3{strdup("U:mono_counter_uint.name:21.5")};
	server.parse_msg(line3.get());

	map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	EXPECT_DOUBLE_EQ(map["mono_counter_uint.name|statistic=count"], 1);
}

TEST(Spectatord, ParseTimer)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line1{strdup("t:timer.name:.001")};
	char_ptr line2{strdup("t:timer.name:.001")};
	server.parse_msg(line1.get());
	server.parse_msg(line2.get());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 2);
	EXPECT_DOUBLE_EQ(map["timer.name|statistic=count"], 2);
	EXPECT_DOUBLE_EQ(map["timer.name|statistic=totalTime"], 0.002);
	EXPECT_DOUBLE_EQ(map["timer.name|statistic=max"], 0.001);
}

// The value "a" hashes (xxHash64, seed 0) to register index 27 (R1B) with rho 1, per the
// shared cross-language test vectors. base64("a") == "YQ==".
TEST(Spectatord, ParseDistinctSketchBase64)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line{strdup("S:dcs.name:YQ==")};
	EXPECT_FALSE(server.parse_msg(line.get()).has_value());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
	EXPECT_DOUBLE_EQ(map["dcs.name|distinct=R1B|statistic=distinct"], 1);
}

// The precomputed-hash variant must land in the same register as the base64 variant.
TEST(Spectatord, ParseDistinctSketchPrecomputedHash)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line{strdup("s:dcs.name:15154266338359012955")};
	EXPECT_FALSE(server.parse_msg(line.get()).has_value());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["dcs.name|distinct=R1B|statistic=distinct"], 1);
}

TEST(Spectatord, ParseDistinctSketchInvalidBase64)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line{strdup("S:dcs.name:not valid base64!")};
	auto err = server.parse_msg(line.get());
	EXPECT_TRUE(err.has_value());
}

// An empty value (base64 of zero bytes) is valid: it hashes the empty byte sequence, which the
// shared vectors map to register R19 (index 25) with rho 1, matching record("") in the other
// clients so sketches merge.
TEST(Spectatord, ParseDistinctSketchEmptyValue)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	char_ptr line{strdup("S:dcs.name:")};
	EXPECT_FALSE(server.parse_msg(line.get()).has_value());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["dcs.name|distinct=R19|statistic=distinct"], 1);
}

TEST(Spectatord, ParseLeadingEmptyLines)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};
	char_ptr line{strdup("\n\nc:x:1\n\nc:x:2")};
	auto err = server.parse_msg(line.get());
	EXPECT_FALSE(err.has_value());
	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 2);
	EXPECT_EQ(map.count("spectatord.parseErrors|statistic=count"), 0);
	EXPECT_DOUBLE_EQ(map["x|statistic=count"], 3);
}

TEST(Spectatord, ParseMultilineWithErrors)
{
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};
	char_ptr line{strdup("c:counter.name:10\nc:counter.name\nc:counter.name:20\n:1\n")};
	auto err = server.parse_msg(line.get());
	ASSERT_TRUE(err.has_value());
	EXPECT_NE(err->find('\n'), std::string::npos);  // "Missing name\nExpecting separator..."
	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 2);
	EXPECT_DOUBLE_EQ(map["spectatord.parseErrors|statistic=count"], 2);
	EXPECT_DOUBLE_EQ(map["counter.name|statistic=count"], 30);
}


}  // namespace

TEST(Spectatord, ParseMalformedTagSectionDoesNotThrow)
{
	// A tag section that ends the line (no '=' and no value separator) used to
	// walk `pos` past the end of the view. Must report an error, never throw.
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	for (const auto* bad : {"c:name,", "c:name,,", "t:name,k", "g:name,k=", "d:name,k=v", "c:a,"})
	{
		char_ptr line{strdup(bad)};
		std::optional<std::string> err;
		EXPECT_NO_THROW(err = server.parse_msg(line.get())) << "threw on '" << bad << "'";
		EXPECT_TRUE(err.has_value()) << "expected a parse error for '" << bad << "'";
	}
}

namespace
{
auto an_id(std::string_view name) -> spectator::Id { return spectator::Id::Of(name); }
}  // namespace

TEST(IdCache, HitMissAndTypeIsPartOfTheKey)
{
	spectatord::id_cache cache{10};
	EXPECT_NE(cache.put('c', "a,b=1", an_id("a")), nullptr);
	auto hit = cache.get('c', "a,b=1");
	ASSERT_NE(hit, nullptr);
	EXPECT_EQ(*hit, an_id("a"));
	EXPECT_EQ(cache.get('t', "a,b=1"), nullptr) << "the line type must be part of the key";

	// inserting must not be counted as a hit, or the hit rate is meaningless

	const auto stats = cache.get_stats();
	EXPECT_EQ(stats.size, 1u);
	EXPECT_EQ(stats.hits, 1u);
	EXPECT_EQ(stats.bytes, 5u + spectatord::id_cache::kEntryOverhead);
}

TEST(IdCache, EntryLimitAndCumulativeByteBudget)
{
	spectatord::id_cache cache{2};
	cache.put('c', "one", an_id("one"));
	cache.put('c', "two", an_id("two"));
	cache.put('c', "three", an_id("three"));
	EXPECT_EQ(cache.get_stats().size, 2u) << "entry limit not applied";
	EXPECT_EQ(cache.get_stats().rejected, 1u);

	spectatord::id_cache by_bytes{8};
	for (auto i = 0; i < 8; ++i)
	{
		by_bytes.put('c', std::string(500, 'x') + std::to_string(i), an_id("x"));
	}
	const auto stats = by_bytes.get_stats();
	EXPECT_LE(stats.bytes, 8 * spectatord::id_cache::kBytesPerEntry) << "byte budget is not cumulative";
	EXPECT_LT(stats.size, 8u);
	EXPECT_GT(stats.rejected, 0u);
}

TEST(IdCache, AdmissionIsFirstCome)
{
	spectatord::id_cache cache{1};
	cache.put('c', "first", an_id("first"));
	cache.put('c', "second", an_id("second"));
	ASSERT_NE(cache.get('c', "first"), nullptr) << "the incumbent was evicted";
	EXPECT_EQ(cache.get('c', "second"), nullptr) << "a second id was admitted past the limit";
}

TEST(IdCache, CapacityZeroDisablesEverything)
{
	spectatord::id_cache cache{0};
	cache.put('c', "a", an_id("a"));
	EXPECT_EQ(cache.get('c', "a"), nullptr);
	EXPECT_EQ(cache.get_stats().size, 0u);
}

TEST(IdCache, KeyIsBytesSoAnyTypeCharIsSafe)
{
	// The type indexes an array of 256 maps, so it has to be treated as
	// unsigned: a high byte arrives as a negative char on this platform.
	spectatord::id_cache cache{4};
	const char high = static_cast<char>(0x80);
	EXPECT_EQ(cache.get(high, "x"), nullptr);
	EXPECT_NE(cache.put(high, "x", an_id("x")), nullptr);
	ASSERT_NE(cache.get(high, "x"), nullptr);
	EXPECT_EQ(cache.get('c', "x"), nullptr) << "0x80 and 'c' must not share a map";
}

TEST(IdCache, ByteBudgetBoundary)
{
	// capacity 1 allows 512 key bytes, of which kEntryOverhead is accounted
	// per entry, so 448 bytes fit exactly and 449 do not
	const auto room = spectatord::id_cache::kBytesPerEntry - spectatord::id_cache::kEntryOverhead;
	{
		spectatord::id_cache cache{1};
		EXPECT_NE(cache.put('c', std::string(room, 'x'), an_id("fits")), nullptr) << "an exactly fitting id was refused";
		EXPECT_EQ(cache.get_stats().rejected, 0u);
	}
	{
		spectatord::id_cache cache{1};
		EXPECT_EQ(cache.put('c', std::string(room + 1, 'x'), an_id("too big")), nullptr) << "one byte over was accepted";
		EXPECT_EQ(cache.get_stats().size, 0u);
		EXPECT_EQ(cache.get_stats().rejected, 1u);
	}
}
TEST(Registry, LookupFastPathKeepsGaugeTtlSemantics)
{
	auto logger = Logger();
	auto cfg = GetConfiguration();
	cfg->meter_ttl = absl::Minutes(15);
	spectator::Registry registry{std::move(cfg), logger};
	const auto id = spectator::Id::Of("fastpath.gauge");

	registry.GetGauge(id, absl::Seconds(5));
	EXPECT_EQ(registry.GetGauge(id)->GetTtl(), absl::Seconds(5)) << "an implicit ttl overwrote the explicit one";

	// an explicit ttl refreshes it even when the gauge already exists
	registry.GetGauge(id, absl::Seconds(30));
	EXPECT_EQ(registry.GetGauge(id)->GetTtl(), absl::Seconds(30)) << "the ttl was not refreshed";
	auto named = 0;
	for (const auto* g : registry.Gauges())
	{
		if (std::string{g->MeterId().Name().Get()} == "fastpath.gauge") ++named;
	}
	EXPECT_EQ(named, 1) << "a second gauge was created for the same id";
}

TEST(Spectatord, ParseCacheHitSkipsInterning)
{
	// The point of the cache is that a repeated line does no identity work at
	// all. Counting cache hits cannot show that, because the counter rises even
	// if the caller ignores the lookup; the string pool can, because a hit must
	// not intern anything.
	auto logger = Logger();
	spectator::Registry registry{GetConfiguration(), logger};
	test_server server{&registry};

	auto pool_accesses = []
	{
		auto s = spectator::string_pool_stats();
		return s.hits + s.misses;
	};

	char_ptr first{strdup("c:interning.probe,app=a,zone=b:1")};
	const auto before_miss = pool_accesses();
	EXPECT_FALSE(server.parse_msg(first.get()).has_value());
	const auto miss_cost = pool_accesses() - before_miss;
	EXPECT_GT(miss_cost, 0u) << "the first sighting should have interned the name and tags";

	char_ptr repeat{strdup("c:interning.probe,app=a,zone=b:2")};
	const auto before_hit = pool_accesses();
	EXPECT_FALSE(server.parse_msg(repeat.get()).has_value());
	EXPECT_EQ(pool_accesses(), before_hit) << "a cached line still interned: the cache result is not being used";

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["interning.probe|app=a|statistic=count|zone=b"], 3);
}

TEST(Spectatord, IdCacheClearsOnlyOnceSaturated)
{
	spectatord::id_cache cache{2};
	cache.put('c', "a", spectator::Id{"a", spectator::Tags{}});
	EXPECT_FALSE(cache.clear_if_saturated()) << "a cache with room must be left alone";
	EXPECT_EQ(cache.get_stats().size, 1u);
	EXPECT_NE(cache.get('c', "a"), nullptr);

	cache.put('c', "b", spectator::Id{"b", spectator::Tags{}});
	EXPECT_EQ(cache.get_stats().size, 2u);
	EXPECT_TRUE(cache.clear_if_saturated()) << "a full cache must be dropped";

	auto stats = cache.get_stats();
	EXPECT_EQ(stats.size, 0u);
	EXPECT_EQ(stats.bytes, 0u);
	EXPECT_EQ(stats.clears, 1u);
	EXPECT_EQ(cache.get('c', "a"), nullptr);
}

TEST(Spectatord, IdCacheClearingLetsNewIdsInAgain)
{
	// The decay this guards against: a cache that filled with ids which are no
	// longer being reported must not refuse new ones for the life of the
	// process.
	spectatord::id_cache cache{2};
	cache.put('c', "old1", spectator::Id{"old1", spectator::Tags{}});
	cache.put('c', "old2", spectator::Id{"old2", spectator::Tags{}});
	EXPECT_EQ(cache.put('c', "fresh", spectator::Id{"fresh", spectator::Tags{}}), nullptr)
	    << "a full cache rejects new ids";

	EXPECT_TRUE(cache.clear_if_saturated());
	EXPECT_NE(cache.put('c', "fresh", spectator::Id{"fresh", spectator::Tags{}}), nullptr)
	    << "after a clear, new ids are admitted again";
	EXPECT_NE(cache.get('c', "fresh"), nullptr);
}

TEST(Spectatord, IdCacheClearIsHarmlessWhenDisabled)
{
	spectatord::id_cache cache{0};
	EXPECT_FALSE(cache.clear_if_saturated());
	EXPECT_EQ(cache.get_stats().clears, 0u);
}

TEST(Spectatord, IdCacheClearDoesNotChangeWhatIsRecorded)
{
	// A clear may only affect speed, never where a measurement lands.
	spectator::Registry registry{GetConfiguration(), Logger()};
	test_server server{&registry};

	char_ptr first{strdup("c:clear.probe,tag=v:1")};
	EXPECT_FALSE(server.parse_msg(first.get()).has_value());

	// The server's cache is far from full, so clear_if_saturated() would do
	// nothing here; clear unconditionally so the line below really does run
	// against an emptied cache.
	server.clear_cache_for_test();

	char_ptr second{strdup("c:clear.probe,tag=v:1")};
	EXPECT_FALSE(server.parse_msg(second.get()).has_value());

	auto map = server.measurements();
	EXPECT_DOUBLE_EQ(map["clear.probe|statistic=count|tag=v"], 2)
	    << "the same line must still reach the same meter after a clear";
}

TEST(Spectatord, IdCacheDoesNotClearWhileItStillHasRoom)
{
	// clear_if_saturated() and put() must agree on what "full" means. They
	// used to disagree: the clear reserved a fixed 512 bytes of headroom while
	// put() charged the id's actual length, so a cache holding long ids was
	// thrown away while it was still admitting new ones.
	// capacity 2 -> 1024 accounted bytes. One 600-byte id charges 664, leaving
	// room for a short entry, but less than the flat 512 the clear reserves.
	spectatord::id_cache cache{2};
	const std::string long_name(600, 'x');
	ASSERT_NE(cache.put('c', long_name, spectator::Id{long_name, spectator::Tags{}}), nullptr);
	ASSERT_EQ(cache.get_stats().size, 1u);

	EXPECT_FALSE(cache.clear_if_saturated())
	    << "the cache is neither at its entry limit nor out of bytes";
	EXPECT_EQ(cache.get_stats().size, 1u) << "a cache with room was thrown away";
	EXPECT_NE(cache.put('c', "short", spectator::Id{"short", spectator::Tags{}}), nullptr)
	    << "and put() proves there was room all along";
}

TEST(Spectatord, MeterCacheSizeZeroDisablesTheRegistryFastPath)
{
	// meter_cache_size is documented as a complete off switch, so it must also
	// turn off the registry's lookup-first path. Nothing else asserted this.
	auto cfg = GetConfiguration();
	cfg->meter_cache_size = 0;
	spectator::Registry off{std::move(cfg), Logger()};
	EXPECT_FALSE(off.MetersForTest().lookup_fast_path);

	auto on_cfg = GetConfiguration();
	on_cfg->meter_cache_size = 10;
	spectator::Registry on{std::move(on_cfg), Logger()};
	EXPECT_TRUE(on.MetersForTest().lookup_fast_path);

	// and the daemon still records correctly with it off
	auto zero = GetConfiguration();
	zero->meter_cache_size = 0;
	spectator::Registry registry{std::move(zero), Logger()};
	test_server server{&registry};
	char_ptr line{strdup("c:off.switch,k=v:4")};
	EXPECT_FALSE(server.parse_msg(line.get()).has_value());
	EXPECT_DOUBLE_EQ(server.measurements()["off.switch|k=v|statistic=count"], 4);
}
