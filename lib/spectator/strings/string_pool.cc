#include "string_pool.h"
#include "valid_chars.inc"

namespace spectator
{

#include "valid_chars.inc"

auto StringPool::Intern(const char* string, size_t len) noexcept -> StrRef
{
	absl::MutexLock lock(&table_mutex_);
	String s{string, len};
	auto it = table_.find(s);
	if (it != table_.end())
	{
		stats_.hits++;
		return it->second;
	}
	auto* copy = static_cast<char*>(malloc(len + 1));
	for (auto i = 0u; i < len; ++i)
	{
		auto ch = static_cast<uint_fast8_t>(string[i]);
		copy[i] = kAtlasChars[ch];
	}
	copy[len] = '\0';

	StrRef ref{copy};
	s.s = copy;
	auto added = table_.insert({s, ref});
	if (added.second)
	{
		stats_.alloc_size += len + 1;  // null terminator
		stats_.misses++;
		stats_.table_size++;
	}
	else
	{
		// it's a cache hit (invalid chars made the first lookup fail)
		stats_.hits++;
		free(copy);
	}
	return added.first->second;
}

StringPool::~StringPool()
{
	absl::MutexLock lock(&table_mutex_);
	for (const auto& kv : table_)
	{
		free(const_cast<char*>(kv.first.s));
	}
}

auto the_str_pool() noexcept -> StringPool&
{
	static auto* the_pool = new StringPool();
	return *the_pool;
}

auto intern_str(const char* string) -> StrRef { return the_str_pool().Intern(string); }

auto intern_str(const std::string& string) -> StrRef { return the_str_pool().Intern(string.c_str(), string.length()); }

auto intern_str(std::string_view string) -> StrRef { return the_str_pool().Intern(string.data(), string.length()); }

auto string_pool_stats() -> StringPoolStats { return the_str_pool().Stats(); }

}  // namespace spectator
