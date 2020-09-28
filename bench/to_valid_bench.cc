#include <benchmark/benchmark.h>
#include <array>

static constexpr std::array<char, 128> kAtlasChars128 = {
    {'_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '-', '.', '_', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_', '_',
     '_', '_', '_', '_', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
     'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y',
     'Z', '_', '_', '_', '^', '_', '_', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
     'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
     'x', 'y', 'z', '_', '_', '_', '~', '_'}};
static constexpr std::array<char, 96> kAtlasChars96 = {
    {'_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '-',
     '.', '_', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_', '_',
     '_', '_', '_', '_', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
     'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
     'X', 'Y', 'Z', '_', '_', '_', '^', '_', '_', 'a', 'b', 'c', 'd', 'e',
     'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
     't', 'u', 'v', 'w', 'x', 'y', 'z', '_', '_', '_', '~', '_'}};
static constexpr std::array<char, 256> kAtlasChars256 = {
    {'_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '-', '.', '_', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_', '_',
     '_', '_', '_', '_', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
     'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y',
     'Z', '_', '_', '_', '^', '_', '_', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
     'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
     'x', 'y', 'z', '_', '_', '_', '~', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
     '_'}};

static void fix_string_small(std::string* s) {
  for (char& c : *s) {
    auto ch = static_cast<int_fast8_t>(c);
    if (ch < 0) {
      ch = '_';
    } else {
      c = kAtlasChars128[ch];
    }
  }
}

static void fix_string_two(std::string* s) {
  for (char& c : *s) {
    auto ch = static_cast<int_fast8_t>(c);
    if (ch < 32) {
      c = '_';
    } else {
      ch -= 32;
      c = kAtlasChars96[ch];
    }
  }
}

static void fix_string_full(std::string* s) {
  for (char& c : *s) {
    auto ch = static_cast<uint_fast8_t>(c);
    c = kAtlasChars256[ch];
  }
}

static void BM_IfSmallTable(benchmark::State& state) {
  std::string tag = "abcdefghijkl.asdf";
  std::string tag2 = "abcdefghijkl#asdf";

  for (auto _ : state) {
    fix_string_small(&tag);
    fix_string_small(&tag2);
  }
}

static void BM_TwoCondsTable(benchmark::State& state) {
  std::string tag = "abcdefghijkl.asdf";
  std::string tag2 = "abcdefghijkl#asdf";

  for (auto _ : state) {
    fix_string_two(&tag);
    fix_string_two(&tag2);
  }
}

static void BM_FullTable(benchmark::State& state) {
  std::string tag = "abcdefghijkl.asdf";
  std::string tag2 = "abcdefghijkl#asdf";

  for (auto _ : state) {
    fix_string_full(&tag);
    fix_string_full(&tag2);
  }
}

BENCHMARK(BM_FullTable);
BENCHMARK(BM_IfSmallTable);
BENCHMARK(BM_TwoCondsTable);

BENCHMARK_MAIN();

// clang-11 on linux
// -----------------------------------------------------------
// Benchmark                 Time             CPU   Iterations
//-----------------------------------------------------------
// BM_FullTable           16.3 ns         16.3 ns     41977102
// BM_IfSmallTable        16.6 ns         16.6 ns     42850745
// BM_TwoCondsTable       17.9 ns         17.9 ns     38787539

// g++-10 on linux
// -----------------------------------------------------------
// Benchmark                 Time             CPU   Iterations
//-----------------------------------------------------------
// BM_FullTable           19.5 ns         19.5 ns     35053205
// BM_IfSmallTable        16.0 ns         16.0 ns     42237693
// BM_TwoCondsTable       24.4 ns         24.4 ns     27564716

// Apple clang-11.0.3
// -----------------------------------------------------------
// Benchmark                 Time             CPU   Iterations
//-----------------------------------------------------------
// BM_FullTable           10.6 ns         10.6 ns     64411646
// BM_IfSmallTable        15.8 ns         15.7 ns     45013761
// BM_TwoCondsTable       19.9 ns         19.9 ns     35180299
