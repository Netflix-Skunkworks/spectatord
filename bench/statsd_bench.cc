#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "spectatord.h"

class dummy_server : public spectatord::Server {
 public:
  explicit dummy_server(spectator::Registry* registry)
      : spectatord::Server(false, 0, 0, "", registry) {}
  void parse_line(char* line) { parse_statsd(line); }
};

static void bench(benchmark::State& state, char* line) {
  auto logger = spdlog::get("bench");
  spectator::Registry registry(std::make_unique<spectator::Config>(), logger);
  dummy_server s{&registry};
  for (auto _ : state) {
    s.parse_line(line);
  }
}

static void bench_counter_notags(benchmark::State& state) {
  char* line = strdup("counter.name:0.001|c");
  bench(state, line);
  free(line);
}

static void bench_counter_tags(benchmark::State& state) {
  char* line = strdup("counter.name:10|c|#system-agent,country=ar");
  bench(state, line);
  free(line);
}

static void bench_sampled_timer(benchmark::State& state) {
  char* line = strdup("timer.name:2|ms|@0.1|#system-agent,country=ar");
  bench(state, line);
  free(line);
}

static void bench_sampled_hist(benchmark::State& state) {
  char* line = strdup("hist.name:2|h|@0.1|#system-agent,country=ar");
  bench(state, line);
  free(line);
}

static void bench_sampled_hist_100x(benchmark::State& state) {
  char* line = strdup("hist.name:2|h|@0.01|#system-agent,country=ar");
  bench(state, line);
  free(line);
}

auto logger = spdlog::stdout_color_mt("bench");
BENCHMARK(bench_counter_tags);
BENCHMARK(bench_counter_notags);
BENCHMARK(bench_sampled_hist);
BENCHMARK(bench_sampled_hist_100x);
BENCHMARK(bench_sampled_timer);
BENCHMARK_MAIN();
