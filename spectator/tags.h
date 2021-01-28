#include "string_intern.h"
#include <algorithm>
#include <array>
#include <memory>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <ostream>

namespace spectator {
struct Tag {
  StrRef key;
  StrRef value;
};

class Tags {
  using value_type = Tag;
  using size_type = size_t;
  using const_iterator = const value_type*;
  using iterator = value_type*;

  static constexpr size_type kSmallTags = 8;
  size_type size_ = 0;

  using entries_t = std::array<Tag, kSmallTags>;
  // when this is a small tag set it uses
  // entries_ otherwise we store the capacity_
  union {
    entries_t entries_{};
    size_type capacity_;
  } U;

  Tag* begin_;

 public:
  Tags() noexcept : begin_(&U.entries_[0]) {}
  Tags(Tags&& o) noexcept : size_{o.size_}, begin_{o.begin_} {
    if (is_large()) {
      // take ownership of the memory
      o.begin_ = nullptr;
      U.capacity_ = o.U.capacity_;
    } else {
      begin_ = &U.entries_[0];
      std::copy(o.begin(), o.end(), begin());
    }
  }

  Tags(const Tags& o) : size_{o.size_} {
    if (is_large()) {
      begin_ = static_cast<Tag*>(malloc(sizeof(Tag) * size_));
      U.capacity_ = size_;
    } else {
      begin_ = &U.entries_[0];
    }
    static_assert(std::is_trivially_copyable<Tag>::value);
    std::copy(o.begin(), o.end(), begin());
  }

  auto operator=(const Tags&) -> Tags& = delete;

  auto operator=(Tags&& o) noexcept -> Tags& {
    size_ = o.size_;
    if (is_large()) {
      begin_ = o.begin_;
      o.begin_ = nullptr;
    } else {
      begin_ = &U.entries_[0];
      std::copy(o.begin(), o.end(), begin());
    }
    return *this;
  }

  ~Tags() noexcept {
    if (is_large()) {
      free(begin_);
    }
  }

  Tags(std::initializer_list<std::pair<std::string_view, std::string_view>> vs)
      : begin_(&U.entries_[0]) {
    for (const auto& pair : vs) {
      add(pair.first, pair.second);
    }
  }

  [[nodiscard]] auto is_large() const -> bool { return size_ > kSmallTags; }

  auto add(const char* k, const char* v) -> void {
    insert(intern_str(k), intern_str(v));
  }

  auto add(std::string_view k, std::string_view v) -> void {
    insert(intern_str(k), intern_str(v));
  }

  auto add(StrRef k, StrRef v) -> void { insert(k, v); }

  [[nodiscard]] auto hash() const -> size_t {
    size_t h = 0U;
    const auto* it = begin();
    while (it != end()) {
      const auto& entry = *it++;
      h += (std::hash<StrRef>()(entry.key) << 1U) ^
           std::hash<StrRef>()(entry.value);
    }
    return h;
  }

  auto add_all(const Tags& source) -> void {
    const auto* it = source.begin();
    while (it != source.end()) {
      const auto& entry = *it++;
      insert(entry.key, entry.value);
    }
  }

  auto operator==(const Tags& that) const -> bool {
    if (size_ != that.size_) {
      return false;
    }
    if (empty()) {
      return true;
    }

    const auto& mine = begin();
    const auto& theirs = that.begin();
    return memcmp(mine, theirs, sizeof(Tag) * size()) == 0;
  }

  [[nodiscard]] auto has(const StrRef& key) const -> bool {
    const auto* it = std::lower_bound(
        begin(), end(), key,
        [](const Tag& tag, const StrRef& k) { return tag.key < k; });
    return it != end() && it->key == key;
  }

  [[nodiscard]] auto at(const StrRef& key) const -> StrRef {
    const auto* it = std::lower_bound(
        begin(), end(), key,
        [](const Tag& tag, const StrRef& k) { return tag.key < k; });
    if (it == end() || it->key != key) {
      return {};
    }
    return it->value;
  }

  [[nodiscard]] auto size() const -> size_type { return size_; }

  [[nodiscard]] auto capacity() const -> size_type {
    return is_large() ? U.capacity_ : kSmallTags;
  }

  [[nodiscard]] auto empty() const -> bool { return size_ == 0; }

  [[nodiscard]] auto begin() const -> const_iterator { return begin_; }

  [[nodiscard]] auto end() const -> const_iterator { return begin_ + size_; }

  [[nodiscard]] auto begin() -> iterator { return begin_; }

  [[nodiscard]] auto end() -> iterator { return begin_ + size_; }

 private:
  auto insert(StrRef k, StrRef v) -> void {
    auto* it = std::lower_bound(
        begin(), end(), k,
        [](const Tag& tag, const StrRef& k) { return tag.key < k; });

    if (it != end()) {
      if (it->key == k) {
        it->value = v;
        return;
      }

      if (size() == capacity()) {
        auto diff = std::distance(begin(), it);
        reallocate();
        it = begin() + diff;
      }

      // move all elements to the right to make room
      // for the new element at position `it`
      std::copy_backward(it, end(), end() + 1);
    } else if (size() == capacity()) {
      reallocate();
      it = end();
    }
    *it = {k, v};
    ++size_;
  }

  auto reallocate() -> void {
    // grow the allocated memory by 4 since the number of tags is usually
    // small
    auto c = capacity();
    auto capacity_bytes = (c + 4) * sizeof(Tag);
    if (!is_large()) {
      begin_ = static_cast<Tag*>(malloc(capacity_bytes));
      std::copy(&U.entries_[0], &U.entries_[size_], begin());
      U.capacity_ = c + 4;
    } else {
      begin_ = static_cast<Tag*>(realloc(begin(), capacity_bytes));
      U.capacity_ += 4;
    }
  }
};

inline auto operator<<(std::ostream& os, const Tags& tags) -> std::ostream& {
  return os << fmt::format("{}", tags);
}
}  // namespace spectator

template <>
struct fmt::formatter<spectator::Tag> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // formatter for Ids
  template <typename FormatContext>
  auto format(const spectator::Tag& tag, FormatContext& context) {
    return fmt::format_to(context.out(), "{}->{}", tag.key, tag.value);
  }
};
