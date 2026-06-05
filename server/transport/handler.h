#pragma once
#include <functional>
#include <optional>
#include <string>

namespace spectatord
{
using handler_t = std::function<std::optional<std::string>(char*)>;
}  // namespace spectatord
