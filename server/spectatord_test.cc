#include <fmt/ostream.h>
#include "local.h"
#include "util/logger.h"
#include "spectatord.h"

#include "gtest/gtest.h"

namespace {

using spectatord::get_measurement;
using spectatord::Logger;

std::unique_ptr<spectator::Config> GetConfiguration() {
  return std::make_unique<spectator::Config>();
}

std::string id_to_string(const spectator::Id& id) {
  std::string res = id.Name().Get();
  const auto& tags = id.GetTags();
  std::map<std::string, std::string> sorted_tags;
  for (const auto& kv : tags) {
    sorted_tags[kv.key.Get()] = kv.value.Get();
  }

  for (const auto& pair : sorted_tags) {
    res += fmt::format("|{}={}", pair.first, pair.second);
  }
  return res;
}

std::map<std::string, double> measurements_to_map(
    const std::vector<spectator::Measurement>& measurements) {
  auto res = std::map<std::string, double>();
  for (const auto& m : measurements) {
    res[id_to_string(m.id)] = m.value;
  }
  return res;
}

class test_server : public spectatord::Server {
 public:
  explicit test_server(spectator::Registry* registry)
      : Server(0, 0, spectatord::kSocketNameDgram, registry),
        registry_{registry} {}
  std::optional<std::string> parse_msg(char* msg) { return parse(msg); }
  std::optional<std::string> test_parse_statsd(char* buffer) {
    return parse_statsd(buffer);
  }

  std::map<std::string, double> measurements() {
    auto ms = registry_->Measurements();
    return measurements_to_map(ms);
  }

 private:
  spectator::Registry* registry_;
};

struct free_deleter {
  template <typename T>
  void operator()(T* p) const {
    std::free(const_cast<std::remove_const_t<T>*>(p));
  }
};
using char_ptr = std::unique_ptr<char, free_deleter>;

void test_statsd(char* buffer, const std::map<std::string, double>& expected) {
  auto logger = Logger();
  spectator::Registry registry{GetConfiguration(), logger};
  test_server server{&registry};

  server.test_parse_statsd(buffer);
  auto actual = server.measurements();

  ASSERT_EQ(actual.size(), expected.size());
  for (const auto& id_value : expected) {
    auto& id = id_value.first;
    auto it = actual.find(id);
    if (it != actual.end()) {
      auto actual_value = it->second;
      EXPECT_DOUBLE_EQ(id_value.second, actual_value)
          << "Wrong value for " << id;
    } else {
      ADD_FAILURE() << "Unable to find " << id;
    }
  }
}

TEST(Spectatord, Statsd_Ctr) {
  char_ptr ctr{strdup("page.views:1|c\n")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"page.views|statistic=count", 1.0}};
  test_statsd(ctr.get(), expected);
}

TEST(Spectatord, Statsd_Multiline) {
  // end without a newline
  char_ptr ctr{strdup("page.views:1|c\npage.views:3|c\npage.views:5|c")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 3},
      {"page.views|statistic=count", 9.0}};
  test_statsd(ctr.get(), expected);

  // end with a newline
  ctr.reset(strdup("page.views:1|c\npage.views:3|c\npage.views:5|c\n"));
  test_statsd(ctr.get(), expected);

  // empty lines
  ctr.reset(strdup("page.views:1|c\n\npage.views:3|c\n\npage.views:5|c\n\n"));
  test_statsd(ctr.get(), expected);
}

TEST(Spectatord, Statsd_GaugeNoTags) {
  char_ptr g{strdup("fuel.level:0.5|g")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"fuel.level|statistic=gauge", 0.5}};
  test_statsd(g.get(), expected);
}

TEST(Spectatord, Statsd_GaugeTagsDlft) {
  char_ptr g{strdup("custom.metric:60|g|#shell")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"custom.metric|shell=1|statistic=gauge", 60.0}};
  test_statsd(g.get(), expected);
}

TEST(Spectatord, Statsd_HistogramAsDs) {
  char_ptr h{strdup("song.length:240|h|#region:east")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"song.length|region=east|statistic=count", 1},
      {"song.length|region=east|statistic=totalAmount", 240},
      {"song.length|region=east|statistic=max", 240},
      {"song.length|region=east|statistic=totalOfSquares", 240 * 240}};
  test_statsd(h.get(), expected);
}

TEST(Spectatord, Statsd_SampledHistogram) {
  char_ptr h{strdup("song.length:240|h|@0.5")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"song.length|statistic=count", 2},
      {"song.length|statistic=totalAmount", 480},
      {"song.length|statistic=totalOfSquares", 240 * 240 * 2},
      {"song.length|statistic=max", 240}};
  test_statsd(h.get(), expected);
}

TEST(Spectatord, DISABLED_SetCardinality) {
  char_ptr s{strdup("users.uniques:1234|s")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"users.uniques|statistic=gauge", 1}};
  test_statsd(s.get(), expected);
}

TEST(Spectatord, Statsd_CounterTags) {
  char_ptr c{strdup("users.online:10|c|#country:china")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"users.online|country=china|statistic=count", 10}};
  test_statsd(c.get(), expected);
}

TEST(Spectatord, Statsd_Timer) {
  char_ptr timer{strdup("req.latency:350|ms|#country:us")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"req.latency|country=us|statistic=count", 1},
      {"req.latency|country=us|statistic=totalTime", 0.35},
      {"req.latency|country=us|statistic=totalOfSquares", 0.35 * 0.35},
      {"req.latency|country=us|statistic=max", 0.35}};
  test_statsd(timer.get(), expected);
}

TEST(Spectatord, Statsd_SampledTimer) {
  char_ptr timer{strdup("req.latency:350|ms|@0.1|#country:us")};
  std::map<std::string, double> expected{
      {"spectatord.parsedCount|statistic=count", 1},
      {"req.latency|country=us|statistic=count", 10},
      {"req.latency|country=us|statistic=totalOfSquares", (0.35 * 0.35) * 10},
      {"req.latency|country=us|statistic=totalTime", 3.5},
      {"req.latency|country=us|statistic=max", 0.35}};
  test_statsd(timer.get(), expected);
}

TEST(Spectatord, ParseOneTag) {
  auto logger = Logger();
  char_ptr line{strdup("my.name,foo=bar:42.0")};
  std::string err_msg;
  auto measurement = *(get_measurement(line.get(), &err_msg));
  logger->info("Got {} = {}", measurement.id, measurement.value);
  EXPECT_DOUBLE_EQ(measurement.value, 42.0);
  EXPECT_EQ(measurement.id,
            spectator::Id("my.name", spectator::Tags{{"foo", "bar"}}));
  EXPECT_TRUE(err_msg.empty());
}

TEST(Spectatord, ParseMultipleTags) {
  auto logger = Logger();
  char_ptr line{strdup("n,foo=bar,k=v1,k2=v2:2")};
  std::string err_msg;
  auto measurement = *get_measurement(line.get(), &err_msg);
  logger->info("Got {} = {}", measurement.id, measurement.value);
  EXPECT_DOUBLE_EQ(measurement.value, 2.0);
  EXPECT_EQ(measurement.id,
            spectator::Id("n", spectator::Tags{
                                   {"foo", "bar"}, {"k", "v1"}, {"k2", "v2"}}));
  EXPECT_TRUE(err_msg.empty());
}

TEST(Spectatord, ParseNoTags) {
  auto logger = Logger();
  char_ptr line{strdup("n:1")};
  std::string err_msg;
  auto measurement = *get_measurement(line.get(), &err_msg);
  logger->info("Got {} = {}", measurement.id, measurement.value);
  EXPECT_DOUBLE_EQ(measurement.value, 1.0);
  EXPECT_EQ(measurement.id, spectator::Id("n", spectator::Tags{}));
}

TEST(Spectatord, ParseMissingName) {
  auto logger = Logger();
  char_ptr line{strdup(":1")};
  std::string err_msg;
  auto measurement = get_measurement(line.get(), &err_msg);
  EXPECT_FALSE(measurement);
  logger->info("Got err_msg = {}", err_msg);
  EXPECT_FALSE(err_msg.empty());
}

TEST(Spectatord, ParseMissingValue) {
  auto logger = Logger();
  char_ptr line{strdup("name:")};
  std::string err_msg;
  auto measurement = get_measurement(line.get(), &err_msg);
  logger->info("Got err_msg = {}", err_msg);
  EXPECT_FALSE(measurement);
  EXPECT_FALSE(err_msg.empty());
}

TEST(Spectatord, ParseIgnoringStuffAtTheEnd) {
  auto logger = Logger();
  char_ptr line{strdup("name:123abc")};
  std::string err_msg;
  auto measurement = *get_measurement(line.get(), &err_msg);
  EXPECT_DOUBLE_EQ(measurement.value, 123.0);
  EXPECT_EQ(measurement.id, spectator::Id("name", spectator::Tags{}));
  logger->info("Got err_msg = {}", err_msg);
  EXPECT_FALSE(err_msg.empty());
}

TEST(Spectatord, ParseCounter) {
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

TEST(Spectatord, ParseMultiline) {
  auto logger = Logger();
  spectator::Registry registry{GetConfiguration(), logger};
  test_server server{&registry};

  char_ptr line{
      strdup("c:counter.name:10\nc:counter.name:20\nc:counter.name:12")};
  server.parse_msg(line.get());

  auto map = server.measurements();
  EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 3);
  EXPECT_DOUBLE_EQ(map["counter.name|statistic=count"], 42);
}

TEST(Spectatord, ParseTimer) {
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

TEST(Spectatord, ParseGauge) {
  auto logger = Logger();
  spectator::Registry registry{GetConfiguration(), logger};
  test_server server{&registry};

  char_ptr line{strdup("g:gauge.name:1.234")};
  server.parse_msg(line.get());

  auto map = server.measurements();
  EXPECT_DOUBLE_EQ(map["spectatord.parsedCount|statistic=count"], 1);
  EXPECT_DOUBLE_EQ(map["gauge.name|statistic=gauge"], 1.234);
}

TEST(Spectatord, ParseGaugeTtl) {
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

TEST(Spectatord, ParseDistSummary) {
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
}  // namespace
