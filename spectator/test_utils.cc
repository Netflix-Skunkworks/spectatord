#include <fmt/format.h>
#include "test_utils.h"

auto id_to_string(const spectator::Id& id) -> std::string {
  std::string res = id.Name().Get();
  const auto& tags = id.GetTags();
  std::map<std::string, std::string> sorted_tags;

  for (const auto& kv : tags) {
    sorted_tags[kv.key.Get()] = kv.value.Get();
  }

  for (const auto& pair : sorted_tags) {
    res += fmt::format("|{}={}", pair.first, pair.second);
  }
  return res;
}

auto measurements_to_map(
    const std::vector<spectator::Measurement>& measurements)
    -> std::map<std::string, double> {
  auto res = std::map<std::string, double>();
  for (const auto& m : measurements) {
    res[id_to_string(m.id)] = m.value;
  }
  return res;
}
