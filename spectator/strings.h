#pragma once

#include <string>

namespace spectator {

auto PathFromUrl(std::string_view url) noexcept -> std::string;

}  // namespace spectator
