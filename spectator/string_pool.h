#pragma once

#include "string_intern.h"
#include "absl/synchronization/mutex.h"
#include "tsl/hopscotch_map.h"
#include "xxh3.h"

namespace spectator {

struct String {
  const char* s;
  size_t len;
};

struct StringHasher {
  size_t operator()(const String& str) const {
    return XXH3_64bits(str.s, str.len);
  }
};

struct StringComparer {
  bool operator()(const String& s1, const String& s2) const {
    if (s1.len != s2.len) return false;
    return std::memcmp(s1.s, s2.s, s1.len) == 0;
  }
};

// A String Pool used for interning Atlas tags
// This class will enforce the atlas charset restrictions
// by setting invalid chars to _
class StringPool {
 public:
  StringPool() = default;
  ~StringPool();
  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;

  StrRef Intern(const char* string) noexcept {
    return Intern(string, std::strlen(string));
  };

  StrRef Intern(const char* string, size_t len) noexcept;

  StringPoolStats Stats() noexcept {
    absl::MutexLock lock(&table_mutex_);
    return stats_;
  }

 private:
  absl::Mutex table_mutex_;
  using table_t =
      tsl::hopscotch_map<String, StrRef, StringHasher, StringComparer>;
  table_t table_ GUARDED_BY(table_mutex_);
  StringPoolStats stats_ GUARDED_BY(table_mutex_){};
};

}  // namespace spectator