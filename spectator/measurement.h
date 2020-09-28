#pragma once

#include "id.h"
#include <ostream>
#include <vector>

namespace spectator {

class Measurement {
 public:
  Measurement(const Id& id_param, double value_param) noexcept
      : id{id_param}, value{value_param} {}

  // We do not own the id_param so temporary references will cause problems.
  // This could happen if the user tries something like:
  // Measurement(id.withTag(...), value);
  Measurement(Id&& error, double value_param) noexcept = delete;

  const Id& id;
  double value;

  bool operator==(const Measurement& other) const {
    return std::abs(value - other.value) < 1e-9 && id == other.id;
  }
};

inline std::ostream& operator<<(std::ostream& os, const Measurement& m) {
  os << "Measurement{" << m.id << "," << m.value << "}";
  return os;
}

using Measurements = std::vector<Measurement>;

}  // namespace spectator
