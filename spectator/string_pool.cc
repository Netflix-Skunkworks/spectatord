#include "string_pool.h"
#include "valid_chars.inc"

namespace spectator {

#include "valid_chars.inc"

StrRef StringPool::Intern(const char* string, size_t len) noexcept {
  absl::MutexLock lock(&table_mutex_);
  String s{string, len};
  auto it = table_.find(s);
  if (it != table_.end()) {
    stats_.hits++;
    return it->second;
  }
  auto* copy = static_cast<char*>(malloc(len + 1));
  for (auto i = 0u; i < len; ++i) {
    auto ch = static_cast<uint_fast8_t>(string[i]);
    copy[i] = kAtlasChars[ch];
  }
  copy[len] = '\0';

  StrRef ref{copy};
  s.s = copy;
  auto added = table_.insert({s, ref});
  if (added.second) {
    stats_.alloc_size += len + 1;  // null terminator
    stats_.misses++;
    stats_.table_size++;
  } else {
    // it's a cache hit (invalid chars made the first lookup fail)
    stats_.hits++;
    free(copy);
  }
  return added.first->second;
}

StringPool::~StringPool() {
  absl::MutexLock lock(&table_mutex_);
  for (const auto& kv : table_) {
    free(const_cast<char*>(kv.first.s));
  }
}

StringPool& the_str_pool() noexcept {
  static auto* the_pool = new StringPool();
  return *the_pool;
}

StrRef intern_str(const char* string) { return the_str_pool().Intern(string); }

StrRef intern_str(const std::string& string) {
  return the_str_pool().Intern(string.c_str(), string.length());
}

StrRef intern_str(std::string_view string) {
  return the_str_pool().Intern(string.data(), string.length());
}

StringPoolStats string_pool_stats() { return the_str_pool().Stats(); }

}  // namespace spectator
