#pragma once

#include <atomic>

namespace spectator {

/// Atomically add a delta to an atomic double
/// equivalent to fetch_add for integer types
inline auto add_double(std::atomic<double>* n, double delta) -> void {
  double current;
  do {
    current = n->load(std::memory_order_relaxed);
  } while (!n->compare_exchange_weak(
      current, n->load(std::memory_order_relaxed) + delta));
}

/// Atomically set the max value of an atomic number
template <typename T>
inline auto update_max(std::atomic<T>* n, T value) -> void {
  T current;
  do {
    current = n->load(std::memory_order_relaxed);
  } while (value > current && !n->compare_exchange_weak(current, value));
}

}  // namespace spectator
