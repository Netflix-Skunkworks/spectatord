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

  static Id Of(std::string_view name, Tags tags = {}) {
    return Id(name, std::move(tags));
  }

  bool operator==(const Id& rhs) const noexcept;

  StrRef Name() const noexcept;

  const Tags& GetTags() const noexcept;

  Id WithTag(StrRef key, StrRef value) const;

  Id WithTags(StrRef k1, StrRef v1, StrRef k2, StrRef v2) const;

  Id WithTags(Tags&& extra_tags) const;

  Id WithStat(StrRef stat) const { return WithTag(refs().statistic(), stat); };

  Id WithDefaultStat(StrRef stat) const {
    if (tags_.has(refs().statistic())) {
      return *this;
    }
    return WithStat(stat);
  }

  friend std::ostream& operator<<(std::ostream& os, const Id& id) {
    return os << fmt::format("{}", id);
  }

  friend struct std::hash<Id>;

  friend struct std::hash<std::shared_ptr<Id>>;

 private:
  StrRef name_;
  Tags tags_;
  mutable size_t hash_;

  size_t Hash() const noexcept {
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
  size_t operator()(const spectator::Id& id) const { return id.Hash(); }
};

template <>
struct hash<spectator::Tags> {
  size_t operator()(const spectator::Tags& tags) const { return tags.hash(); }
};

template <>
struct hash<shared_ptr<spectator::Id>> {
  size_t operator()(const shared_ptr<spectator::Id>& id) const {
    return id->Hash();
  }
};

template <>
struct equal_to<shared_ptr<spectator::Id>> {
  bool operator()(const shared_ptr<spectator::Id>& lhs,
                  const shared_ptr<spectator::Id>& rhs) const {
    return *lhs == *rhs;
  }
};

}  // namespace std

template <>
struct fmt::formatter<spectator::Id> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // formatter for Ids
  template <typename FormatContext>
  auto format(const spectator::Id& id, FormatContext& context) {
    return fmt::format_to(context.out(), "Id({}, {})", id.Name(), id.GetTags());
  }
};
