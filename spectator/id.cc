#include "id.h"
#include <utility>

namespace spectator {

StrRef Id::Name() const noexcept { return name_; }

const Tags& Id::GetTags() const noexcept { return tags_; }

bool Id::operator==(const Id& rhs) const noexcept {
  return name_ == rhs.name_ && tags_ == rhs.tags_;
}

Id Id::WithTag(StrRef key, StrRef value) const {
  // Create a copy
  Tags tags{GetTags()};
  tags.add(key, value);
  return Id(Name(), tags);
}

Id Id::WithTags(StrRef k1, StrRef v1, StrRef k2, StrRef v2) const {
  // Create a copy
  Tags tags{GetTags()};
  tags.add(k1, v1);
  tags.add(k2, v2);
  return Id(Name(), tags);
}

Id Id::WithTags(Tags&& extra_tags) const {
  Tags tags{GetTags()};
  tags.add_all(extra_tags);
  return Id(Name(), std::move(tags));
}

}  // namespace spectator
