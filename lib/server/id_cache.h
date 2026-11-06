#pragma once

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "spectator/meters/id.h"

#include <atomic>
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
// Keyed on the id text alone. The line type is deliberately not part of the
// key: get_measurement uses the type only to parse the value, never to parse
// the name and tags, so the same text always yields the same Id whatever the
// type. "c:a,k=v:1" and "g:a,k=v:1" therefore share one entry, and the caller
// still resolves its own kind of meter from it.
//
// An Id is a value, so a cached entry cannot go stale: the registry is still
// consulted for every line, and remains the only owner of meter lifetimes.
//
// The cache holds at most `capacity` entries and at most `capacity` times
// kBytesPerEntry of accounted bytes, where an entry is charged its key length
// plus kEntryOverhead for everything else it carries. Beyond either limit ids are parsed the usual
// way, which bounds what runaway cardinality can cost: nothing in the protocol
// limits the length of a name or tag value, so an entry limit alone would let
// a client sending very long ids use far more memory than one sending ordinary
// ones. A capacity of 0 disables the cache entirely, and also turns off the
// registry's lookup-first fast path, so that the flag is a complete off
// switch rather than a partial one. Admission is first come,
// first served; entries are never evicted individually, so a burst of
// cardinality cannot push out the ids that are actually being reported.
//
// Because nothing is evicted, ids that stop being reported would otherwise
// hold their slots for the life of the process, and a cache that has filled
// with them would refuse every new id for ever. clear_if_saturated(), called
// periodically by the server's upkeep thread, drops the whole cache once it
// is full so the live working set can re-form. Nothing depends on this for
// correctness: a cached Id is a value, so a stale entry is only a wasted
// slot.
//
// Clearing is not free. An id that is still being reported costs a miss and a
// re-insert, so clearing a cache whose entries are all live is pure loss; on a
// capture whose working set exceeded the capacity it measured around 15%
// slower than simply refusing new ids. It only pays when the entries are dead,
// which is why it is gated on saturation rather than run on a timer: a cache
// with room is never touched, and that is the usual case.
class id_cache
{
   public:
	// Average bytes allowed per cached entry. Ids of a few hundred bytes are
	// ordinary, so normal traffic stays well inside this and is limited by the
	// entry count instead; the byte budget is a backstop against ids long
	// enough that the entry count alone would not bound memory.
	static constexpr size_t kBytesPerEntry = 1024;
	// Charged on top of the key text, to cover everything an entry costs beyond
	// the key's own characters: the std::string's heap block and header, the
	// Id with its inline array of eight tags, the shared_ptr control block, and
	// the map slot at its load factor. Measured against real ids from three
	// services, an entry costs 1.7 to 1.9 times its key length plus 64, with
	// the overhead beyond the key itself between 254 and 317 bytes; 256 keeps
	// the accounting close to what the allocator actually reports.
	static constexpr size_t kEntryOverhead = 256;

	struct stats
	{
		size_t size;
		size_t capacity;
		size_t bytes;
		uint64_t hits;
		uint64_t misses;
		uint64_t rejected;
		uint64_t clears;
	};

	// Whether anything is cached at all. Callers check this before deriving a
	// key, which costs a scan of the id text, so that a daemon running with the
	// cache switched off does not pay for machinery it is not using.
	[[nodiscard]] auto enabled() const -> bool { return capacity_ != 0; }

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
	auto get(std::string_view id) -> std::shared_ptr<const spectator::Id>
	{
		if (capacity_ == 0)
		{
			return nullptr;
		}
		// A shared lock: this is the only operation on the hot path and it does
		// not modify the map. The hit and miss counters are relaxed atomics so
		// that keeping them does not force exclusive access; they are only ever
		// reported, never used to make a decision.
		absl::ReaderMutexLock lock{&mutex_};
		auto it = map_.find(id);
		if (it == map_.end())
		{
			misses_.fetch_add(1, std::memory_order_relaxed);
			return nullptr;
		}
		hits_.fetch_add(1, std::memory_order_relaxed);
		return it->second;
	}

	// Caches parsed and returns the cached copy, or nullptr if a limit refused
	// it, in which case the caller keeps using the Id it already parsed.
	auto put(std::string_view id, const spectator::Id& parsed) -> std::shared_ptr<const spectator::Id>
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
		auto result = map_.emplace(std::string{id}, std::move(entry));
		if (result.second)
		{
			++size_;
			bytes_ += entry_bytes;
		}
		return result.first->second;
	}

	// Drops every entry, but only once the cache is full and is therefore
	// turning new ids away. Returns true if the cache was cleared.
	//
	// Saturation is tested as a level rather than an edge: "is there room for
	// another entry right now". Clearing on the rejection counter having moved
	// instead would throw the cache away because one oversized id was refused
	// by the byte budget, while the cache was nearly empty.
	//
	// "Room" means room for the smallest entry put() could admit, so that the
	// two agree: the cache is declared full only when put() would refuse
	// everything. Reserving a flat kBytesPerEntry instead would clear a cache
	// of long ids while put() was still accepting shorter ones.
	auto clear_if_saturated() -> bool
	{
		if (capacity_ == 0)
		{
			return false;
		}
		// Freeing tens of thousands of entries takes milliseconds, so the map
		// is swapped out under the lock and destroyed after it is released.
		// Every metric line needs this mutex.
		decltype(map_) discarded;
		{
			absl::MutexLock lock{&mutex_};
			if (size_ < capacity_ && bytes_ + kEntryOverhead <= max_bytes_)
			{
				return false;
			}
			map_.swap(discarded);
			size_ = 0;
			bytes_ = 0;
			++clears_;
		}
		return true;
	}

	// Unconditional clear, so a test can exercise the path without having to
	// fill the cache first.
	void clear_for_test()
	{
		absl::MutexLock lock{&mutex_};
		map_.clear();
		size_ = 0;
		bytes_ = 0;
	}

	// Exposed for tests; a future change can report these as status metrics.
	auto get_stats() const -> stats
	{
		absl::ReaderMutexLock lock{&mutex_};
		return stats{size_,
		             capacity_,
		             bytes_,
		             hits_.load(std::memory_order_relaxed),
		             misses_.load(std::memory_order_relaxed),
		             rejected_,
		             clears_};
	}

   private:
	// Lookup by string_view does not allocate.
	const size_t capacity_;
	const size_t max_bytes_;
	mutable absl::Mutex mutex_;
	size_t size_ ABSL_GUARDED_BY(mutex_) = 0;
	size_t bytes_ ABSL_GUARDED_BY(mutex_) = 0;
	// Not guarded: relaxed counters, so that a cache hit needs only a shared
	// lock on the map.
	std::atomic<uint64_t> hits_{0};
	std::atomic<uint64_t> misses_{0};
	uint64_t rejected_ ABSL_GUARDED_BY(mutex_) = 0;
	uint64_t clears_ ABSL_GUARDED_BY(mutex_) = 0;
	absl::flat_hash_map<std::string, std::shared_ptr<const spectator::Id>> map_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace spectatord
