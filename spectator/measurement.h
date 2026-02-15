#pragma once

#include "id.h"
#include <ostream>
#include <vector>
#include <fmt/core.h>

namespace spectator
{

class Measurement
{
   public:
	Measurement(const Id& id_param, double value_param) noexcept : id{id_param}, value{value_param} {}

	// We do not own the id_param so temporary references will cause problems.
	// This could happen if the user tries something like:
	// Measurement(id.withTag(...), value);
	Measurement(Id&& error, double value_param) noexcept = delete;

	const Id& id;
	double value;

	auto operator==(const Measurement& other) const -> bool
	{
		return std::abs(value - other.value) < 1e-9 && id == other.id;
	}
};

using Measurements = std::vector<Measurement>;

}  // namespace spectator

// formatter: Measurement{Id(c, {id->2, statistic->count}),1}
template <>
struct fmt::formatter<spectator::Measurement> : formatter<std::string_view>
{
	static auto format(const spectator::Measurement& m, format_context& ctx) -> format_context::iterator
	{
		return fmt::format_to(ctx.out(), "Measurement{{{}, {}}}", m.id, m.value);
	}
};

inline auto operator<<(std::ostream& os, const spectator::Measurement& m) -> std::ostream&
{
	return os << fmt::format("{}", m);
}
