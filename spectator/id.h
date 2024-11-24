#pragma once

#include "common_refs.h"
#include "string_intern.h"
#include "tags.h"
#include <memory>
#include <ostream>
#include <string>

namespace spectator {

class Id {
 public:
  Id(StrRef name, Tags tags) noexcept
      : name_(name), tags_(std::move(tags)), hash_(0U) {}

  Id(std::string_view name, Tags tags) noexcept
      : name_(intern_str(name)), tags_(std::move(tags)), hash_(0U) {}

  static auto Of(std::string_view name, Tags tags = {}) -> Id {
    return Id(name, std::move(tags));
  }

  auto operator==(const Id& rhs) const noexcept -> bool {
    return name_ == rhs.name_ && tags_ == rhs.tags_;
  }

  [[nodiscard]] auto Name() const noexcept -> StrRef { return name_; }

  [[nodiscard]] auto GetTags() const noexcept -> const Tags& { return tags_; }

  [[nodiscard]] auto WithTag(StrRef key, StrRef value) const -> Id {
    // Create a copy
    Tags tags{GetTags()};
    tags.add(key, value);
    return Id(Name(), tags);
  }

  [[nodiscard]] auto WithTags(StrRef k1, StrRef v1, StrRef k2, StrRef v2) const
      -> Id {
    // Create a copy
    Tags tags{GetTags()};
    tags.add(k1, v1);
    tags.add(k2, v2);
    return Id(Name(), tags);
  }

  [[nodiscard]] auto WithTags(Tags&& extra_tags) const -> Id {
    Tags tags{GetTags()};
    tags.add_all(extra_tags);
    return Id(Name(), std::move(tags));
  }

  [[nodiscard]] auto WithStat(StrRef stat) const -> Id {
    return WithTag(refs().statistic(), stat);
  };

  [[nodiscard]] auto WithDefaultStat(StrRef stat) const -> Id {
    if (tags_.has(refs().statistic())) {
      return *this;
    }
    return WithStat(stat);
  }

  friend auto operator<<(std::ostream& os, const Id& id) -> std::ostream& {
    return os << fmt::format("{}", id);
  }

  friend struct std::hash<Id>;

  friend struct std::hash<std::shared_ptr<Id>>;

 private:
  StrRef name_;
  Tags tags_;
  mutable size_t hash_;

  [[nodiscard]] auto Hash() const noexcept -> size_t {
    if (hash_ == 0) {
      // compute hash code, and reuse it
      hash_ = tags_.hash() ^ std::hash<StrRef>()(name_);
    }
    return hash_;
  }
};

}  // namespace spectator

namespace std {
template <>
struct hash<spectator::Id> {
  auto operator()(const spectator::Id& id) const -> size_t { return id.Hash(); }
};

template <>
struct hash<spectator::Tags> {
  auto operator()(const spectator::Tags& tags) const -> size_t {
    return tags.hash();
  }
};

template <>
struct hash<shared_ptr<spectator::Id>> {
  auto operator()(const shared_ptr<spectator::Id>& id) const -> size_t {
    return id->Hash();
  }
};

template <>
struct equal_to<shared_ptr<spectator::Id>> {
  auto operator()(const shared_ptr<spectator::Id>& lhs,
                  const shared_ptr<spectator::Id>& rhs) const -> bool {
    return *lhs == *rhs;
  }
};

}  // namespace std

// formatter: Id(c, {id->2, statistic->count})
template <> struct fmt::formatter<spectator::Id>: formatter<std::string_view> {
  static auto format(const spectator::Id& id, format_context& ctx) -> format_context::iterator {
    return fmt::format_to(ctx.out(), "Id({}, {})", id.Name(), id.GetTags());
  }
};
