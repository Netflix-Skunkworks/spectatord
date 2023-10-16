#pragma once

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "spectator/registry.h"
#include <chrono>
#include <memory>

namespace spectatord {
namespace detail {
template <typename V>
struct cache_entry {
  mutable std::chrono::steady_clock::time_point updated;
  std::unique_ptr<V> v;
};
}  // namespace detail

template <typename V>
class expiring_cache {
 public:
  using supplier_t = std::unique_ptr<V> (*)(spectator::Registry*,
                                            spectator::Id);
  explicit expiring_cache(supplier_t supplier) : supplier_{supplier} {}

  V* get_or_create(spectator::Registry* registry, spectator::Id k) {
    absl::MutexLock lock{&cache_mutex_};

    auto it = cache_.find(k);
    if (it != cache_.end()) {
      auto& entry = it->second;
      entry.updated = std::chrono::steady_clock::now();
      return entry.v.get();
    }

    detail::cache_entry<V> entry{std::chrono::steady_clock::now(),
                                 supplier_(registry, k)};
    auto* ret = entry.v.get();
    cache_.emplace(std::make_pair(std::move(k), std::move(entry)));
    return ret;
  }

  std::pair<size_t, size_t> expire() {
    static constexpr auto kExpireAfter = std::chrono::seconds{120};
    absl::MutexLock lock{&cache_mutex_};
    auto size = cache_.size();
    auto expired = 0U;
    auto now = std::chrono::steady_clock::now();
    auto it = cache_.begin();
    auto end = cache_.end();
    while (it != end) {
      auto& entry = it->second;
      auto age = now - entry.updated;
      if (age > kExpireAfter) {
        it = cache_.erase(it);
        ++expired;
      } else {
        ++it;
      }
    }
    return std::make_pair(size, expired);
  }

 private:
  using table_t = tsl::hopscotch_map<spectator::Id, detail::cache_entry<V>>;
  absl::Mutex cache_mutex_;
  table_t cache_ ABSL_GUARDED_BY(cache_mutex_);
  supplier_t supplier_;
};

}  // namespace spectatord
