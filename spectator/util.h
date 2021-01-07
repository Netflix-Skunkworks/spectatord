#pragma once

namespace spectator {

template <typename T>
auto restrict(T amount, T min, T max) -> T {
  auto r = amount;
  if (r > max) {
    r = max;
  } else if (r < min) {
    r = min;
  }
  return r;
}

}  // namespace spectator
