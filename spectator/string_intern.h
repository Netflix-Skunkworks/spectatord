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
  bool operator==(const StrRef& rhs) const { return data == rhs.data; }
  bool operator!=(const StrRef& rhs) const { return data != rhs.data; }
  [[nodiscard]] const char* Get() const { return data; }
  [[nodiscard]] size_t Length() const { return std::strlen(data); }

  bool operator<(StrRef rhs) const { return strcmp(data, rhs.Get()) < 0; }

 private:
  explicit StrRef(const char* s) : data{s} {}
  const char* data{nullptr};
  friend StringPool;
  friend std::hash<StrRef>;
};

StrRef intern_str(const char* string);
StrRef intern_str(const std::string& string);
StrRef intern_str(std::string_view string);
StringPoolStats string_pool_stats();

}  // namespace spectator

namespace std {
template <>
struct hash<spectator::StrRef> {
  size_t operator()(const spectator::StrRef& ref) const {
    return hash<const char*>{}(ref.data);
  }
};
}  // namespace std

template <>
struct fmt::formatter<spectator::StrRef> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const spectator::StrRef& s, FormatContext& context) {
    std::string_view str_view{s.Get(), s.Length()};
    return fmt::formatter<std::string_view>::format(str_view, context);
  }
};
