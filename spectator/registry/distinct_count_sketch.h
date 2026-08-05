#pragma once

#include "spectator/meters/id.h"
#include "spectator/meters/max_gauge.h"
#include "spectator/strings/common_refs.h"
#include "spectator/strings/string_intern.h"
#include "registry.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "xxhash.h"

namespace spectator
{

// Estimates the number of distinct values seen during a step interval using a HyperLogLog
// sketch decomposed into a fixed set of per-register max gauges, tagged statistic=distinct
// and distinct=R##. The registers merge across sources by taking the per-register max, and
// the cardinality estimate is computed at query time on the backend.
//
// The hash and register encoding must match the spectator-java reference implementation
// exactly so that sketches published by different clients merge correctly. This is verified
// against a shared set of test vectors (see distinct_count_sketch_test.cc).
class DistinctCountSketch
{
   public:
	// Number of HLL registers. Fixed global constant: a distinct=R## tag must mean the same
	// partition of the hash space everywhere or sketches from different sources cannot merge.
	static constexpr int kRegisters = 64;

	// Number of low bits of the hash used to select the register (log2(kRegisters)).
	static constexpr int kIndexBits = 6;

	// Mask to extract the register index from the low bits of the hash.
	static constexpr uint64_t kIndexMask = kRegisters - 1;

	// Maximum rho, produced when every bit beyond the index is zero (1 + 64 - kIndexBits).
	static constexpr int kMaxRho = 64 - kIndexBits + 1;

	// The register tags are formatted as "R%02X"; two hex digits only fit indices < 256.
	static_assert(kRegisters <= 256, "register tag format R%02X assumes the index fits in two hex digits");

	DistinctCountSketch(Registry* registry, Id id) noexcept : registry_{registry}, id_{std::move(id)} {}

	// Record a distinct long value. Hashed as its 8-byte little-endian representation, matching
	// the spectator-java record(long) overload.
	void Record(int64_t value) noexcept
	{
		std::array<uint8_t, 8> bytes{};
		auto v = static_cast<uint64_t>(value);
		for (int i = 0; i < 8; ++i)
		{
			bytes[i] = static_cast<uint8_t>(v >> (8 * i));
		}
		RecordHash(XXH64(bytes.data(), bytes.size(), 0));
	}

	// Record a distinct string value, hashed as its UTF-8 bytes.
	void Record(std::string_view value) noexcept { RecordHash(XXH64(value.data(), value.size(), 0)); }

	// Record a distinct value from its raw bytes.
	void Record(const void* data, size_t len) noexcept { RecordHash(XXH64(data, len, 0)); }

	// Record a value from its precomputed xxHash64 (seed 0). Used by the precomputed-hash path
	// where the caller has already hashed the value.
	void RecordHash(uint64_t hash) noexcept { get_register(register_index(hash))->Update(register_rho(hash)); }

	// Register index for a hash: the low kIndexBits bits.
	static auto register_index(uint64_t hash) noexcept -> int { return static_cast<int>(hash & kIndexMask); }

	// Register value (rho) for a hash: 1 + the number of leading zeros in the bits above the
	// index. When every bit above the index is zero the result is kMaxRho.
	static auto register_rho(uint64_t hash) noexcept -> int
	{
		uint64_t w = hash >> kIndexBits;
		if (w == 0)
		{
			return kMaxRho;
		}
		// The top kIndexBits bits of w are zero, so subtract them back out of the count.
		return __builtin_clzll(w) - kIndexBits + 1;
	}

	auto MeterId() const noexcept -> const Id& { return id_; }

   private:
	Registry* registry_;
	Id id_;

	// Interned register tag values R00..R3F, matching the spectator-java String.format("R%02X").
	static auto register_tags() -> const std::array<StrRef, kRegisters>&
	{
		static const std::array<StrRef, kRegisters> tags = [] {
			std::array<StrRef, kRegisters> t{};
			for (int i = 0; i < kRegisters; ++i)
			{
				char buf[4];
				std::snprintf(buf, sizeof(buf), "R%02X", i);
				t[i] = intern_str(buf);
			}
			return t;
		}();
		return tags;
	}

	auto get_register(int index) -> std::shared_ptr<MaxGauge>
	{
		auto reg_id = id_.WithTags(refs().statistic(), refs().distinct(), refs().distinct(), register_tags()[index]);
		return registry_->GetMaxGauge(std::move(reg_id));
	}
};

}  // namespace spectator
