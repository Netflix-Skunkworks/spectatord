#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace spectator
{

auto PercentileBucket(int64_t v) -> int64_t;
auto PercentileBucketIndexOf(int64_t v) -> size_t;
constexpr auto PercentileBucketsLength() -> size_t { return 276; }
auto Percentile(const std::array<int64_t, PercentileBucketsLength()>& counts, double p) -> double;
///
/// Compute a set of percentiles based on the counts for the buckets.
///
/// @param counts
///     Counts for each of the buckets.
///     positions must correspond to the positions of the bucket values.
/// @param pcts
///     Vector with the requested percentile values. The length must be at least
///     1 and the
///     array should be sorted. Each value, {@code v}, should adhere to {@code
///     0.0 <= v <= 100.0}.
/// @param results
///     The calculated percentile values will be written to the results vector.
void Percentiles(const std::array<int64_t, PercentileBucketsLength()>& counts, const std::vector<double>& pcts,
                 std::vector<double>* results);

}  // namespace spectator
