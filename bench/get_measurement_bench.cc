/*

  Test different get_measurement() implementations

  Using the strchr/strpbrk functions could in theory provide benefits over
  manually manipulating pointers

  On linux using g++ 7.5.0:

Run on (4 X 3097.72 MHz CPU s)
CPU Caches:
  L1 Data 32K (x2)
  L1 Instruction 32K (x2)
  L2 Unified 1024K (x2)
  L3 Unified 33792K (x1)
Load Average: 3.41, 1.49, 0.67
------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------
bench_get_measurement_ptr           2976 ns         2976 ns       234024
bench_get_measurement_strchr        2729 ns         2729 ns       252424
bench_get_measurement_strview       2488 ns         2488 ns       284401
*/

#include <benchmark/benchmark.h>
#include "spectatord.h"

using spectator::Id;
using spectatord::measurement;

std::optional<measurement> get_measurement_ptr(
    const spectator::Registry* registry, const char* measurement_str,
    std::string* err_msg) {
  // get name
  auto p = measurement_str;
  while (p && *p != ':' && *p != ',') ++p;
  std::string name{measurement_str, std::size_t(p - measurement_str)};
  if (name.empty()) {
    *err_msg = "Missing name";
    return {};
  }
  spectator::Tags tags{};
  const char* key_ptr = nullptr;
  const char* val_ptr = nullptr;
  while (p && *p != ':') {
    if (*p == ',') {
      // add new key=val pair
      if (key_ptr && val_ptr) {
        std::string key{key_ptr, val_ptr - 1};
        std::string val{val_ptr, p};
        tags.add(std::move(key), std::move(val));
        val_ptr = nullptr;
      }
      key_ptr = ++p;
    } else if (*p == '=') {
      val_ptr = ++p;
    } else {
      ++p;
    }
  }
  // add the last seen tag if any
  if (key_ptr && val_ptr) {
    std::string key{key_ptr, val_ptr - 1};
    std::string val{val_ptr, p};
    tags.add(std::move(key), std::move(val));
  }
  // point p past the last :
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

  auto id = Id::Of(name, std::move(tags));
  return measurement{std::move(id), value};
}

std::optional<measurement> get_measurement_strchr(
    const spectator::Registry* registry, const char* measurement_str,
    std::string* err_msg) {
  // get name (tags are specified with , but are optional)
  auto p = std::strpbrk(measurement_str, ",:");
  if (p == nullptr || p == measurement_str) {
    *err_msg = "Missing name";
    return {};
  }
  std::string name{measurement_str, p};

  // get tags
  spectator::Tags tags{};
  while (*p != ':') {
    ++p;
    auto k = strchr(p, '=');
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

  auto id = Id::Of(std::move(name), std::move(tags));
  return measurement{std::move(id), value};
}

std::optional<measurement> get_measurement_strview(
    const spectator::Registry* registry, const char* measurement_str,
    std::string* err_msg) {
  // get name (tags are specified with , but are optional)
  auto p = std::strpbrk(measurement_str, ",:");
  if (p == nullptr || p == measurement_str) {
    *err_msg = "Missing name";
    return {};
  }
  std::string_view name{
      measurement_str,
      static_cast<std::string_view::size_type>(p - measurement_str)};

  // get tags
  spectator::Tags tags{};
  while (*p != ':') {
    ++p;
    auto k = strchr(p, '=');
    if (k == nullptr) break;
    std::string_view key{p, static_cast<std::string_view::size_type>(k - p)};
    ++k;
    p = strpbrk(k, ",:");
    if (p == nullptr) {
      *err_msg = "Missing value";
      return {};
    }
    std::string_view val{k, static_cast<std::string_view::size_type>(p - k)};
    tags.add(key, val);
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

  auto id = Id::Of(name, std::move(tags));
  return measurement{std::move(id), value};
}

static std::vector<std::string> get_measurements() {
  std::vector<std::string> measurements;
  measurements.reserve(5);
  measurements.emplace_back(
      "spectatord_test.counter,id=1234,foo=some-foo:42.0\n");
  measurements.emplace_back("spectatord_test.timer,id=1234,foo=some-foo:0.5\n");
  measurements.emplace_back("spectatord_test.ds,id=12341234,foo=some-foo:42\n");
  measurements.emplace_back(
      "spectatord_test.max,id=12341234,foo=some-foo:0.12341234\n");
  measurements.emplace_back(
      "spectatord_test.percTimer,id=12341234,tag=bar,tag1=baradfasasdf,"
      "tagadsdf=badsfsadfasdfasdf:12341234.1234\n");
  return measurements;
}

using measurement_fn = std::function<std::optional<measurement>(
    const spectator::Registry*, const char*, std::string*)>;

static void bench_get_measurement_fun(benchmark::State& state,
                                      const measurement_fn& fn) {
  auto logger = spdlog::get("bench");
  spectator::Registry registry(std::make_unique<spectator::Config>(), logger);
  auto ms = get_measurements();
  for (auto _ : state) {
    std::string err;
    for (const auto& m : ms) {
      benchmark::DoNotOptimize(fn(&registry, m.c_str(), &err));
    }
  }
}

static void bench_get_measurement_ptr(benchmark::State& state) {
  bench_get_measurement_fun(state, get_measurement_ptr);
}

static void bench_get_measurement_strchr(benchmark::State& state) {
  bench_get_measurement_fun(state, get_measurement_strchr);
}

static void bench_get_measurement_strview(benchmark::State& state) {
  bench_get_measurement_fun(state, get_measurement_strview);
}

BENCHMARK(bench_get_measurement_ptr);
BENCHMARK(bench_get_measurement_strchr);
BENCHMARK(bench_get_measurement_strview);
BENCHMARK_MAIN();
