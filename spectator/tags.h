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

  using entries_t = Tag[kSmallTags];
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
    } else {
      begin_ = &U.entries_[0];
      std::copy(o.begin(), o.end(), begin());
    }
  }

  Tags(const Tags& o) : size_{o.size_} {
    if (is_large()) {
      begin_ = static_cast<Tag*>(malloc(sizeof(Tag) * size()));
    } else {
      begin_ = &U.entries_[0];
    }
    static_assert(std::is_trivially_copyable<Tag>::value);
    std::copy(o.begin(), o.end(), begin());
  }

  Tags& operator=(const Tags&) = delete;

  Tags& operator=(Tags&& o) noexcept {
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

  [[nodiscard]] bool is_large() const { return size_ > kSmallTags; }

  void add(const char* k, const char* v) {
    insert(intern_str(k), intern_str(v));
  }

  void add(std::string_view k, std::string_view v) {
    insert(intern_str(k), intern_str(v));
  }

  void add(StrRef k, StrRef v) { insert(k, v); }

  [[nodiscard]] size_t hash() const {
    size_t h = 0U;
    const auto* it = begin();
    while (it != end()) {
      const auto& entry = *it++;
      h += (std::hash<StrRef>()(entry.key) << 1U) ^
           std::hash<StrRef>()(entry.value);
    }
    return h;
  }

  void add_all(const Tags& source) {
    const auto* it = source.begin();
    while (it != source.end()) {
      const auto& entry = *it++;
      insert(entry.key, entry.value);
    }
  }

  bool operator==(const Tags& that) const {
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

  [[nodiscard]] bool has(const StrRef& key) const {
    const auto* it = std::lower_bound(
        begin(), end(), key,
        [](const Tag& tag, const StrRef& k) { return tag.key < k; });
    return it != end();
  }

  [[nodiscard]] StrRef at(const StrRef& key) const {
    const auto* it = std::lower_bound(
        begin(), end(), key,
        [](const Tag& tag, const StrRef& k) { return tag.key < k; });
    if (it == end()) {
      return {};
    }
    return it->value;
  }

  [[nodiscard]] size_type size() const { return size_; }

  [[nodiscard]] bool empty() const { return size_ == 0; }

  [[nodiscard]] const_iterator begin() const { return begin_; }

  [[nodiscard]] const_iterator end() const { return begin_ + size_; }

  [[nodiscard]] iterator begin() { return begin_; }

  [[nodiscard]] iterator end() { return begin_ + size_; }

 private:
  void insert(StrRef k, StrRef v) {
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

      // move all elements one to the right
      std::copy_backward(it, end(), end() + 1);
    } else if (size() == capacity()) {
      reallocate();
      it = end();
    }
    *it = {k, v};
    ++size_;
  }

  [[nodiscard]] size_type capacity() const {
    return is_large() ? U.capacity_ : kSmallTags;
  }

  void reallocate() {
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

inline std::ostream& operator<<(std::ostream& os, const Tags& tags) {
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
