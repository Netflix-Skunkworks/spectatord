#pragma once

#include <cstring>
#include <string>
#include <fmt/format.h>

namespace spectator {

class StringPool;

struct StringPoolStats {
  size_t alloc_size;
  size_t table_size;
  uint64_t hits;
  uint64_t misses;
};

class StrRef {
 public:
  StrRef() = default;
  auto operator==(const StrRef& rhs) const -> bool { return data == rhs.data; }
  auto operator!=(const StrRef& rhs) const -> bool { return data != rhs.data; }
  [[nodiscard]] auto Get() const -> const char* { return data; }
  [[nodiscard]] auto Length() const -> size_t { return std::strlen(data); }

  auto operator<(StrRef rhs) const -> bool {
    return strcmp(data, rhs.Get()) < 0;
  }

 private:
  explicit StrRef(const char* s) : data{s} {}
  const char* data{nullptr};
  friend StringPool;
  friend std::hash<StrRef>;
};

auto intern_str(const char* string) -> StrRef;
auto intern_str(const std::string& string) -> StrRef;
auto intern_str(std::string_view string) -> StrRef;
auto string_pool_stats() -> StringPoolStats;

}  // namespace spectator

namespace std {
template <>
struct hash<spectator::StrRef> {
  auto operator()(const spectator::StrRef& ref) const -> size_t {
    return hash<const char*>{}(ref.data);
  }
};
}  // namespace std

template <> struct fmt::formatter<spectator::StrRef>: formatter<std::string_view> {
  auto format(const spectator::StrRef& s, format_context& ctx) const -> format_context::iterator {
    std::string_view str_view{s.Get(), s.Length()};
    return fmt::formatter<std::string_view>::format(str_view, ctx);
  }
};
