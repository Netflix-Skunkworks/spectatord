/*

  Test different line format definitions

  Try to measure the difference of introducing the optional tags with :#
  instead of a comma optionally following the name.
  This allows us to avoid one call to strpbrk in favor of strchr

Run on (16 X 2300 MHz CPU s)
CPU Caches:
  L1 Data 32K (x8)
  L1 Instruction 32K (x8)
  L2 Unified 262K (x8)
  L3 Unified 16777K (x1)
Load Average: 2.50, 2.19, 2.14
---------------------------------------------------------------------
Benchmark                           Time             CPU   Iterations
---------------------------------------------------------------------
bench_get_measurement_orig       4036 ns         4032 ns       169057
bench_get_measurement_new        3951 ns         3947 ns       176396
*/

#include <benchmark/benchmark.h>
#include "spectatord.h"
using spectator::Id;
using spectatord::measurement;

std::optional<measurement> get_measurement_new(const char* measurement_str,
                                               std::string* err_msg) {
  // get name (tags are specified with :# but are optional)
  const char* p = std::strchr(measurement_str, ':');
  if (p == nullptr || p == measurement_str) {
    *err_msg = "Missing name";
    return {};
  }
  std::string name{measurement_str, p};
  spectator::Tags tags{};

  ++p;
  // optionally get tags
  if (*p == '#') {
    while (*p != ':') {
      ++p;
      const char* k = strchr(p, '=');
      if (k == nullptr) break;
      std::string key{p, k};
      ++k;
      p = strpbrk(k, ",:");
      if (p == nullptr) {
        *err_msg = "Missing value";
        return {};
      }
      std::string val{k, p};
      tags.add(std::move(key), std::move(val));
    }
    ++p;
  }

  char* last_char;
  auto value = strtod(p, &last_char);
  if (last_char == p) {
    // unable to parse a double
    *err_msg = "Unable to parse value for measurement";
    return {};
  } else {
    if (*last_char && !isspace(*last_char)) {
      // just a warning
      *err_msg =
          fmt::format("Got {} parsing value, ignoring chars starting at {}",
                      value, last_char);
    }
  }

  return measurement{(Id::Of(name, std::move(tags))), value};
}

std::optional<measurement> get_measurement_orig(const char* measurement_str,
                                                std::string* err_msg) {
  // get name (tags are specified with , but are optional)
  const char* p = std::strpbrk(measurement_str, ",:");
  if (p == nullptr || p == measurement_str) {
    *err_msg = "Missing name";
    return {};
  }
  std::string name{measurement_str, p};
  // get tags
  spectator::Tags tags{};
  while (*p != ':') {
    ++p;
    const char* k = strchr(p, '=');
    if (k == nullptr) break;
    std::string key{p, k};
    ++k;
    p = strpbrk(k, ",:");
    if (p == nullptr) {
      *err_msg = "Missing value";
      return {};
    }
    std::string val{k, p};
    tags.add(std::move(key), std::move(val));
  }

  ++p;
  char* last_char;
  auto value = strtod(p, &last_char);
  if (last_char == p) {
    // unable to parse a double
    *err_msg = "Unable to parse value for measurement";
    return {};
  } else {
    if (*last_char && !isspace(*last_char)) {
      // just a warning
      *err_msg =
          fmt::format("Got {} parsing value, ignoring chars starting at {}",
                      value, last_char);
    }
  }

  return measurement{Id::Of(name, std::move(tags)), value};
}

std::vector<std::string> get_measurements_orig() {
  std::vector<std::string> measurements;
  measurements.reserve(5);
  measurements.emplace_back("spectatord_test.counter:42.0\n");
  measurements.emplace_back(
      "spectatord_test.timer,id=1234,foo=some-foo,tag3=bar3,tag4=aasdfasdfasdf:"
      "0.5\n");
  measurements.emplace_back("spectatord_test.ds,id=12341234,foo=some-foo:42\n");
  measurements.emplace_back(
      "spectatord_test.max,id=12341234,foo=some-foo,statistic=percentile,"
      "another-tag=value:0.12341234\n");
  measurements.emplace_back(
      "spectatord_test.percTimer,id=12341234,tag=bar,tag1=baradfasasdf,"
      "tagadsdf=badsfsadfasdfasdf:12341234.1234\n");
  return measurements;
}

std::vector<std::string> get_measurements_new() {
  std::vector<std::string> measurements = get_measurements_orig();
  for (auto& s : measurements) {
    auto n = s.find(',');
    if (n != std::string::npos) {
      s.replace(n, 1, ":#");
    }
  }
  return measurements;
}

static auto orig = get_measurements_orig();
static auto new_ms = get_measurements_new();

static void bench_get_measurement_orig(benchmark::State& state) {
  for (auto _ : state) {
    std::string err;
    for (const auto& m : orig) {
      benchmark::DoNotOptimize(get_measurement_orig(m.c_str(), &err));
    }
  }
}

static void bench_get_measurement_new(benchmark::State& state) {
  static auto ms = get_measurements_new();
  for (auto _ : state) {
    std::string err;
    for (const auto& m : new_ms) {
      benchmark::DoNotOptimize(get_measurement_new(m.c_str(), &err));
    }
  }
}

BENCHMARK(bench_get_measurement_orig);
BENCHMARK(bench_get_measurement_new);
BENCHMARK_MAIN();