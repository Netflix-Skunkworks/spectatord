#pragma once

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "spectator/meters/id.h"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

namespace spectatord
{

// Caches the Id parsed from the identity portion of a spectator line (the
// metric name and tags), so that repeated measurements for the same id skip
// tag parsing and string interning.
//
// An Id is a value, so a cached entry cannot go stale: the registry is still
// consulted for every line, and remains the only owner of meter lifetimes.
//
// The cache holds at most `capacity` entries and at most `capacity` times
// kBytesPerEntry of key data. Beyond either limit ids are parsed the usual
// way, which bounds what runaway cardinality can cost: nothing in the protocol
// limits the length of a name or tag value, so an entry limit alone would let
// a client sending very long ids use far more memory than one sending ordinary
// ones. A capacity of 0 disables the cache entirely. Admission is first come,
// first served; entries are never evicted, so a burst of cardinality cannot
// push out the ids that are actually being reported.
class id_cache
{
   public:
	// Average key bytes allowed per cached entry. Ids of a few hundred bytes
	// are ordinary, so normal traffic is limited by the entry count.
	static constexpr size_t kBytesPerEntry = 512;
	// Accounts for the map slot, the std::string header and the Id.
	static constexpr size_t kEntryOverhead = 64;

	struct stats
	{
		size_t size;
		size_t capacity;
		size_t bytes;
		uint64_t hits;
		uint64_t misses;
		uint64_t rejected;
	};

	explicit id_cache(size_t capacity)
	    : capacity_{capacity},
	      max_bytes_{capacity > std::numeric_limits<size_t>::max() / kBytesPerEntry
	                     ? std::numeric_limits<size_t>::max()
	                     : capacity * kBytesPerEntry}
	{
	}

	// Returns the Id cached for (type, id text), or nullptr on a miss. The
	// entry is handed out as a shared_ptr rather than a pointer into the map,
	// which would dangle as soon as an insert rehashed it.
	auto get(char type, std::string_view id) -> std::shared_ptr<const spectator::Id>
	{
		if (capacity_ == 0)
		{
			return nullptr;
		}
		absl::MutexLock lock{&mutex_};
		const auto& map = maps_[static_cast<uint8_t>(type)];
		auto it = map.find(id);
		if (it == map.end())
		{
			++misses_;
			return nullptr;
		}
		++hits_;
		return it->second;
	}

	// Caches parsed and returns the cached copy, or nullptr if a limit refused
	// it, in which case the caller keeps using the Id it already parsed.
	auto put(char type, std::string_view id, const spectator::Id& parsed) -> std::shared_ptr<const spectator::Id>
	{
		if (capacity_ == 0)
		{
			return nullptr;
		}
		absl::MutexLock lock{&mutex_};
		const auto entry_bytes = id.size() + kEntryOverhead;
		if (size_ >= capacity_ || bytes_ + entry_bytes > max_bytes_)
		{
			++rejected_;
			return nullptr;
		}
		auto entry = std::make_shared<const spectator::Id>(parsed);
		// Memoise the hash now. Id caches it lazily in a mutable field, and the
		// registry lookup takes its Id by value, so without this every copy
		// taken from the cache would hash the name and tags again.
		(void)std::hash<spectator::Id>{}(*entry);
		auto result = maps_[static_cast<uint8_t>(type)].emplace(std::string{id}, std::move(entry));
		if (result.second)
		{
			++size_;
			bytes_ += entry_bytes;
		}
		return result.first->second;
	}

	// Exposed for tests; a future change can report these as status metrics.
	auto get_stats() const -> stats
	{
		absl::MutexLock lock{&mutex_};
		return stats{size_, capacity_, bytes_, hits_, misses_, rejected_};
	}

   private:
	// One map per line type ('c', 't', ...) so that the type does not need to
	// be part of the key. Lookup by string_view does not allocate.
	const size_t capacity_;
	const size_t max_bytes_;
	mutable absl::Mutex mutex_;
	size_t size_ ABSL_GUARDED_BY(mutex_) = 0;
	size_t bytes_ ABSL_GUARDED_BY(mutex_) = 0;
	uint64_t hits_ ABSL_GUARDED_BY(mutex_) = 0;
	uint64_t misses_ ABSL_GUARDED_BY(mutex_) = 0;
	uint64_t rejected_ ABSL_GUARDED_BY(mutex_) = 0;
	std::array<absl::flat_hash_map<std::string, std::shared_ptr<const spectator::Id>>, 256> maps_
	    ABSL_GUARDED_BY(mutex_);
};

}  // namespace spectatord
