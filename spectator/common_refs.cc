#include "common_refs.h"

namespace spectator {

auto refs() -> Refs& {
  static auto* the_refs = new Refs();
  return *the_refs;
}

}  // namespace spectator
