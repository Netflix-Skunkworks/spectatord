#pragma once

#include "id.h"
#include <ostream>
#include <vector>
#include <fmt/core.h>

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

  auto operator==(const Measurement& other) const -> bool {
    return std::abs(value - other.value) < 1e-9 && id == other.id;
  }
};

inline auto operator<<(std::ostream& os, const Measurement& m) -> std::ostream& {
  return os << fmt::format("{}", m);
}

using Measurements = std::vector<Measurement>;

}  // namespace spectator

template <> struct fmt::formatter<spectator::Measurement> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  // formatter: Measurement{Id(c, {id->2, statistic->count}),1}
  template <typename format_context>
  constexpr auto format(const spectator::Measurement& m, format_context& ctx) const {
    return fmt::format_to(ctx.out(), "Measurement{{{}, {}}}", m.id, m.value);
  }
};
