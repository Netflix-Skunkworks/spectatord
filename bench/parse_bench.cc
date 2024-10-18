#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "spectatord.h"

static std::vector<std::string> get_measurements() {
  std::vector<std::string> measurements;
  measurements.reserve(500);
  for (auto i = 0; i < 100; ++i) {
    measurements.emplace_back(
        fmt::format("1:c:spectatord_test.counter{}:42.0\n", i));
    measurements.emplace_back(
        fmt::format("1:t:spectatord_test.timer,id={},foo=some-foo:0.5\n", i));
    measurements.emplace_back(
        fmt::format("1:d:spectatord_test.ds,id={},foo=some-foo:42\n", i));
    measurements.emplace_back(
        fmt::format("1:m:spectatord_test.max,id={},foo=some-foo:{}\n", i, i));
    measurements.emplace_back(fmt::format(
        "1:T:spectatord_test.percTimer,id={},tag=bar:{}\n", i, i % 10));
  }
  return measurements;
}

class dummy_server : public spectatord::Server {
 public:
  explicit dummy_server(spectator::Registry* registry)
      : spectatord::Server(false, 0, 0, "", registry) {}
  void do_it() {
    auto measurements = get_measurements();
    for (const auto& s : measurements) {
      parse(const_cast<char*>(s.c_str()));
    }
  }
};

static void bench_process_measurements(benchmark::State& state) {
  auto logger = spdlog::get("bench");
  for (auto _ : state) {
    spectator::Registry registry(std::make_unique<spectator::Config>(), logger);
    dummy_server s{&registry};
    s.do_it();
  }
}

auto logger = spdlog::stdout_color_mt("bench");
BENCHMARK(bench_process_measurements);
BENCHMARK_MAIN();
